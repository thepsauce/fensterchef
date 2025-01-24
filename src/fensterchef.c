#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_renderutil.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"
#include "xalloc.h"

/* event mask for the root window */
#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | \
                         XCB_EVENT_MASK_BUTTON_PRESS | \
                         /* when the user adds a monitor (e.g. video
                          * projector), the root window gets a
                          * ConfigureNotify */ \
                         XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | \
                         XCB_EVENT_MASK_PROPERTY_CHANGE | \
                         XCB_EVENT_MASK_FOCUS_CHANGE | \
                         XCB_EVENT_MASK_ENTER_WINDOW)

/* xcb server connection */
xcb_connection_t        *g_dpy;

/* ewmh information */
xcb_ewmh_connection_t   g_ewmh;

/* the screen being used */
uint32_t                g_screen_no;

/* 1 while the window manager is running */
unsigned                g_running;

/* graphis objects with the id referring to the xcb id */
uint32_t                g_stock_objects[STOCK_COUNT];

/* general purpose values */
uint32_t                g_values[6];

/* user notification window */
xcb_window_t            g_notification_window;

/* list of windows */
xcb_window_t            g_window_list_window;

/* Handle an incoming alarm. */
static void alarm_handler(int sig)
{
    (void) sig;
    LOG("triggered alarm: hiding notification window\n");
    xcb_unmap_window(g_dpy, g_notification_window);
    xcb_flush(g_dpy);
}

/* Initialize graphical objects like pens and drawing contexts. */
static int init_stock_objects(void)
{
    xcb_screen_t                                *screen;
    xcb_generic_error_t                         *error;
    xcb_render_color_t                          color;
    xcb_render_pictforminfo_t                   *fmt;
    const xcb_render_query_pict_formats_reply_t *fmt_reply;
    xcb_drawable_t                              root;
    xcb_pixmap_t                                pixmap;
    xcb_rectangle_t                             rect;

    for (uint32_t i = 0; i < STOCK_COUNT; i++) {
        g_stock_objects[i] = xcb_generate_id(g_dpy);
    }

    screen = SCREEN(g_screen_no);

    g_values[0] = screen->black_pixel;
    g_values[1] = screen->white_pixel;
    error = xcb_request_check(g_dpy,
            xcb_create_gc_checked(g_dpy, g_stock_objects[STOCK_GC],
                screen->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, g_values));
    if (error != NULL) {
        ERR("could not create graphics context for notifications: %d\n",
                error->error_code);
        return 1;
    }

    g_values[0] = screen->white_pixel;
    g_values[1] = screen->black_pixel;
    error = xcb_request_check(g_dpy,
            xcb_create_gc_checked(g_dpy, g_stock_objects[STOCK_INVERTED_GC],
                screen->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, g_values));
    if (error != NULL) {
        ERR("could not create inverted graphics context for notifications: %d\n",
                error->error_code);
        return 1;
    }

    fmt_reply = xcb_render_util_query_formats(g_dpy);

    fmt = xcb_render_util_find_standard_format(fmt_reply,
            XCB_PICT_STANDARD_ARGB_32);

    root = screen->root;

    /* create white pen */
    pixmap = xcb_generate_id(g_dpy);
    xcb_create_pixmap(g_dpy, 32, pixmap, root, 1, 1);

    g_values[0] = XCB_RENDER_REPEAT_NORMAL;
    xcb_render_create_picture(g_dpy, g_stock_objects[STOCK_WHITE_PEN],
            pixmap, fmt->id,
            XCB_RENDER_CP_REPEAT, g_values);

    rect.x = 0;
    rect.y = 0;
    rect.width = 1;
    rect.height = 1;

    color.alpha = 0xff00;
    color.red = 0xff00;
    color.green = 0xff00;
    color.blue = 0xff00;
    xcb_render_fill_rectangles(g_dpy, XCB_RENDER_PICT_OP_OVER,
            g_stock_objects[STOCK_WHITE_PEN], color, 1, &rect);

    xcb_free_pixmap(g_dpy, pixmap);

    /* create black pen */
    pixmap = xcb_generate_id(g_dpy);
    xcb_create_pixmap(g_dpy, 32, pixmap, root, 1, 1);

    g_values[0] = XCB_RENDER_REPEAT_NORMAL;
    xcb_render_create_picture(g_dpy, g_stock_objects[STOCK_BLACK_PEN],
            pixmap, fmt->id,
            XCB_RENDER_CP_REPEAT, g_values);

    color.red = 0x0000;
    color.green = 0x0000;
    color.blue = 0x0000;
    xcb_render_fill_rectangles(g_dpy, XCB_RENDER_PICT_OP_OVER,
            g_stock_objects[STOCK_BLACK_PEN], color, 1, &rect);

    xcb_free_pixmap(g_dpy, pixmap);

    return 0;
}

/* Initialize most of fensterchef data and set root window flags.
 *
 * This opens the logging file, opens the xcb connection, initializes ewmh
 * which also initializes the screens.
 *
 * Next, the root window mask is changed to allow substructure redirecting
 * needed to receive map requests and destroy notifications and such.
 *
 * After that, the font drawing and the initial font are initialized and the
 * graphical objects, also called stock objects.
 */
int init_fensterchef(void)
{
    xcb_intern_atom_cookie_t    *cookies;
    xcb_screen_t                *screen;
    xcb_generic_error_t         *error;

    if ((void*) LOG_FILE == (void*) stderr) {
        g_log_file = stderr;
    } else {
        g_log_file = fopen(LOG_FILE, "w");
        if (g_log_file == NULL) {
            g_log_file = stderr;
        }
    }

    g_dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(g_dpy) > 0) {
        ERR("could not create xcb connection\n");
        fclose(g_log_file);
        exit(1);
    }

    cookies = xcb_ewmh_init_atoms(g_dpy, &g_ewmh);
    if (!xcb_ewmh_init_atoms_replies(&g_ewmh, cookies, NULL)) {
        ERR("could not set up ewmh\n");
        fclose(g_log_file);
        exit(1);
    }

#ifdef DEBUG
    log_screens();
#endif

    g_screen_no = 0;
    screen = SCREEN(g_screen_no);

    g_frame_capacity = 10;
    g_frames = xmalloc(sizeof(*g_frames) * g_frame_capacity);
    for (Frame f = 1; f < g_frame_capacity; f++) {
        g_frames[f].window = WINDOW_SENTINEL;
    }
    g_frames[0].window = NULL;
    g_frames[0].x = 0;
    g_frames[0].y = 0;
    g_frames[0].width = screen->width_in_pixels;
    g_frames[0].height = screen->height_in_pixels;

    g_values[0] = ROOT_EVENT_MASK;
    error = xcb_request_check(g_dpy,
            xcb_change_window_attributes_checked(g_dpy, screen->root,
                XCB_CW_EVENT_MASK, g_values));
    if (error != NULL) {
        ERR("could not change root window mask: %d\n", error->error_code);
        return 1;
    }

    init_font_drawing();

    set_font(NULL);

    init_stock_objects();

    g_notification_window = xcb_generate_id(g_dpy);
    g_values[0] = 0x000000;
    error = xcb_request_check(g_dpy, xcb_create_window_checked(g_dpy,
                XCB_COPY_FROM_PARENT, g_notification_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                XCB_CW_BORDER_PIXEL, g_values));
    if (error != NULL) {
        ERR("could not create notification window: %d\n", error->error_code);
        return 1;
    }

    g_window_list_window = xcb_generate_id(g_dpy);
    g_values[0] = 0x000000;
    g_values[1] = XCB_EVENT_MASK_KEY_PRESS;
    error = xcb_request_check(g_dpy, xcb_create_window_checked(g_dpy,
                XCB_COPY_FROM_PARENT, g_window_list_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, g_values));
    if (error != NULL) {
        LOG("could not create window list window\n");
        return 1;
    }

    xcb_grab_button(g_dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, XCB_MOD_MASK_1);

    signal(SIGALRM, alarm_handler);

    return 0;
}

void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* TODO: maybe free all resources to show we care? */
    deinit_font_drawing();
    xcb_disconnect(g_dpy);
    fclose(g_log_file);
    exit(exit_code);
}

/* Show the notification window with given message at given coordinates. */
void set_notification(const FcChar8 *msg, int32_t x, int32_t y)
{
    size_t              msg_len;
    struct text_measure tm;
    xcb_rectangle_t     rect;
    xcb_render_color_t  color;

    msg_len = strlen((char*) msg);
    measure_text(msg, msg_len, &tm);

    tm.total_width += WINDOW_PADDING;

    x -= tm.total_width / 2;
    y -= (tm.ascent - tm.descent + WINDOW_PADDING) / 2;

    g_values[0] = x;
    g_values[1] = y;
    g_values[2] = tm.total_width;
    g_values[3] = tm.ascent - tm.descent + WINDOW_PADDING;
    g_values[4] = 1;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH |
            XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_map_window(g_dpy, g_notification_window);

    rect.x = 0;
    rect.y = 0;
    rect.width = tm.total_width;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    color.alpha = 0xff00;
    color.red = 0xff00;
    color.green = 0xff00;
    color.blue = 0xff00;
    draw_text(g_notification_window, msg, msg_len, color, &rect,
            g_stock_objects[STOCK_BLACK_PEN], WINDOW_PADDING / 2,
            tm.ascent + WINDOW_PADDING / 2);

    alarm(NOTIFICATION_DURATION);
}
