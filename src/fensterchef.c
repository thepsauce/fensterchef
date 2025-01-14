#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

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
                         XCB_EVENT_MASK_POINTER_MOTION | \
                         XCB_EVENT_MASK_PROPERTY_CHANGE | \
                         XCB_EVENT_MASK_FOCUS_CHANGE | \
                         XCB_EVENT_MASK_ENTER_WINDOW)

/* xcb server connection */
xcb_connection_t    *g_dpy;

/* screens */
xcb_screen_t        **g_screens;
uint32_t            g_screen_count;
/* the screen being used */
uint32_t            g_screen_no;

/* 1 while the window manager is running */
int                 g_running;

/* the graphics context used for drawing black text on white background */
xcb_gcontext_t      g_drawing_context;

/* same as g_drawing_context but with inverted colors */
xcb_gcontext_t      g_inverted_context;

/* the font used for rendering */
xcb_font_t          g_font;

/* general purpose values */
uint32_t            g_values[6];

const char          *g_atom_names[ATOM_COUNT] = {
    [WM_PROTOCOLS]      = "WM_PROTOCOLS",
    [WM_DELETE_WINDOW]  = "WM_DELETE_WINDOW",

    [NET_SUPPORTED]     = "NET_SUPPORED",
    [NET_FULLSCREEN]    = "NET_FULLSCREEN",
    [NET_WM_STATE]      = "NET_WM_STATE",
    [NET_ACTIVE]        = "NET_ACTIVE",
};

/* all interned atoms */
xcb_atom_t          g_atoms[ATOM_COUNT];

/* user notification window */
xcb_window_t        g_notification_window;

/* list of windows */
xcb_window_t        g_window_list_window;

/* Handle an incoming alarm. */
static void alarm_handler(int sig)
{
    (void) sig;
    LOG("triggered alarm: hiding notification window\n");
    xcb_unmap_window(g_dpy, g_notification_window);
    xcb_flush(g_dpy);
}

/* Initialize most of fensterchef data and set root window flags. */
int init_fensterchef(void)
{
    xcb_screen_iterator_t       iter;
    xcb_screen_t                *screen;
    xcb_generic_error_t         *error;
    xcb_intern_atom_cookie_t    cookies[ATOM_COUNT];
    xcb_intern_atom_reply_t     *reply;
    const char                  *font_name = "-misc-fixed-*";

    g_dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(g_dpy) > 0) {
        LOG("could not create xcb connection\n");
        exit(1);
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(g_dpy));
    RESIZE(g_screens, iter.rem);
    g_screen_count = 0;
    for (; iter.rem > 0; xcb_screen_next(&iter)) {
        g_screens[g_screen_count++] = iter.data;
    }

    for (uint32_t i = 0; i < ATOM_COUNT; i++) {
        cookies[i] = xcb_intern_atom(g_dpy, 0,
                strlen(g_atom_names[i]), g_atom_names[i]);
    }

    for (uint32_t i = 0; i < ATOM_COUNT; i++) {
        reply = xcb_intern_atom_reply(g_dpy, cookies[i], NULL);
        if (reply == NULL) {
            LOG("failed interning %s\n", g_atom_names[i]);
        } else {
            LOG("interned atom %s : %" PRId32 "\n", g_atom_names[i],
                    reply->atom);
            g_atoms[i] = reply->atom;
            free(reply);
        }
    }

    if (g_screen_count == 0) {
        LOG("no screens to attach to\n");
        return 1;
    }

    g_screen_no = 0;
    screen = g_screens[g_screen_no];

    create_frame(NULL, 0, 0, screen->width_in_pixels, screen->height_in_pixels);

    xcb_change_property(g_dpy, XCB_PROP_MODE_REPLACE, screen->root,
            g_atoms[NET_SUPPORTED], XCB_ATOM_ATOM, 32,
            ATOM_COUNT - FIRST_NET_ATOM, &g_atoms[FIRST_NET_ATOM]);

    g_values[0] = ROOT_EVENT_MASK;
    error = xcb_request_check(g_dpy,
            xcb_change_window_attributes_checked(g_dpy, screen->root,
                XCB_CW_EVENT_MASK, g_values));
    if (error != NULL) {
        LOG("could not change root window mask\n");
        return 1;
    }

    g_drawing_context = xcb_generate_id(g_dpy);
    g_inverted_context = xcb_generate_id(g_dpy);
    g_font = xcb_generate_id(g_dpy);

    error = xcb_request_check(g_dpy, xcb_open_font_checked(g_dpy,
                g_font, strlen(font_name), font_name));
    if (error != NULL) {
        LOG("could not create notification window font\n");
        return 1;
    }

    g_values[0] = screen->black_pixel;
    g_values[1] = screen->white_pixel;
    g_values[2] = g_font;
    error = xcb_request_check(g_dpy,
            xcb_create_gc_checked(g_dpy, g_drawing_context, screen->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, g_values));
    if (error != NULL) {
        LOG("could not create graphics context for notifications\n");
        return 1;
    }

    g_values[0] = screen->white_pixel;
    g_values[1] = screen->black_pixel;
    g_values[2] = g_font;
    error = xcb_request_check(g_dpy,
            xcb_create_gc_checked(g_dpy, g_inverted_context, screen->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, g_values));
    if (error != NULL) {
        LOG("could not create inverted graphics context for notifications\n");
        return 1;
    }

    g_notification_window = xcb_generate_id(g_dpy);
    g_values[0] = 0x000000;
    error = xcb_request_check(g_dpy, xcb_create_window_checked(g_dpy,
                XCB_COPY_FROM_PARENT, g_notification_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                XCB_CW_BORDER_PIXEL, g_values));
    if (error != NULL) {
        LOG("could not create notification window\n");
        return 1;
    }

    g_window_list_window = xcb_generate_id(g_dpy);
    g_values[0] = 0x000000;
    g_values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS;
    error = xcb_request_check(g_dpy, xcb_create_window_checked(g_dpy,
                XCB_COPY_FROM_PARENT, g_window_list_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, g_values));
    if (error != NULL) {
        LOG("could not create window list window\n");
        return 1;
    }

    signal(SIGALRM, alarm_handler);

    return 0;
}

/* Show the notification window with given message at given coordinates. */
void set_notification(const char *msg, int32_t x, int32_t y)
{
    size_t                          msg_len;
    xcb_char2b_t                    *xcb_str = NULL;
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t  *reply;
    uint32_t                        w, h;
    const uint32_t                  padding = 2;

    msg_len = strlen(msg);
    RESIZE(xcb_str, msg_len);
    for (size_t i = 0; i < msg_len; i++) {
        xcb_str[i].byte1 = 0;
        xcb_str[i].byte2 = msg[i];
    }
    cookie = xcb_query_text_extents(g_dpy, g_font, msg_len, xcb_str);
    free(xcb_str);

    reply = xcb_query_text_extents_reply(g_dpy, cookie, NULL);
    if (reply == NULL) {
        LOG("could not get text extent reply for the notification window\n");
        return;
    }

    w = reply->overall_width + padding;
    h = reply->font_ascent + reply->font_descent + padding;
    x -= w / 2;
    y -= h / 2;

    g_values[0] = x;
    g_values[1] = y;
    g_values[2] = w;
    g_values[3] = h;
    g_values[4] = 1;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_map_window(g_dpy, g_notification_window);

    xcb_image_text_8(g_dpy, strlen(msg), g_notification_window, g_drawing_context,
            padding / 2, reply->font_ascent + padding / 2, msg);

    alarm(3);

    free(reply);
}

/* Handle the mapping of a new window. */
void accept_new_window(xcb_window_t win)
{
    Window *window;

    window = create_window(win);
    if (g_cur_frame->window != NULL) {
        hide_window(g_cur_frame->window);
    }
    g_cur_frame->window = window;
    show_window(window);
    set_focus_window(window);
}

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event)
{
    xcb_configure_request_event_t   *request_event;
    Window                          *window;
    Frame                           *frame;

#ifdef DEBUG
    log_event(event, g_log_file);
    fputc('\n', g_log_file);
#endif

    switch ((event->response_type & ~0x80)) {
    case XCB_MAP_REQUEST:
        accept_new_window(((xcb_map_request_event_t*) event)->window);
        break;

    case XCB_UNMAP_NOTIFY:
        window = get_window_of_xcb_window(
                ((xcb_unmap_notify_event_t*) event)->window);
        if (window != NULL && window->visible) {
            window->visible = 0;
            frame = get_frame_of_window(window);
            window = get_next_hidden_window(window);
            frame->window = window;
            if (window != NULL) {
                show_window(window);
            }
        }
        break;

    case XCB_DESTROY_NOTIFY:
        window = get_window_of_xcb_window(
                ((xcb_unmap_notify_event_t*) event)->window);
        if (window != NULL) {
            destroy_window(window);
        }
        break;

    case XCB_CONFIGURE_REQUEST:
        request_event = (xcb_configure_request_event_t*) event;
        g_values[0] = request_event->x;
        g_values[1] = request_event->y;
        g_values[2] = request_event->width;
        g_values[3] = request_event->height;
        g_values[4] = request_event->border_width;
        xcb_configure_window(g_dpy, request_event->window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                g_values);
        break;

    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        break;
    }
}

/* Close the connection to the xcb server. */
void close_connection(void)
{
    xcb_disconnect(g_dpy);
}
