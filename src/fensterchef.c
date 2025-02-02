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

#include "fensterchef.h"
#include "log.h"
#include "util.h"
#include "render_font.h"
#include "screen.h"
#include "xalloc.h"

/* xcb server connection */
xcb_connection_t        *g_dpy;

/* ewmh information */
xcb_ewmh_connection_t   g_ewmh;

/* 1 while the window manager is running */
unsigned                g_running;

/* general purpose values */
uint32_t                g_values[6];

/* Handle an incoming alarm. */
static void alarm_handler(int sig)
{
    (void) sig;
    LOG("triggered alarm: hiding notification window\n");
    xcb_unmap_window(g_dpy, g_screen->notification_window);
    xcb_flush(g_dpy);
}

/* Initialize most of fensterchef data and set root window flags.
 *
 * This opens the logging file, opens the xcb connection, initializes ewmh
 * which also initializes the screens.
 *
 * After that, the font drawing and the initial font are initialized.
 */
int init_fensterchef(void)
{
    xcb_intern_atom_cookie_t    *cookies;

#ifdef DEBUG
    if ((void*) LOG_FILE == (void*) stderr) {
        g_log_file = stderr;
    } else {
        g_log_file = fopen(LOG_FILE, "w");
        if (g_log_file == NULL) {
            g_log_file = stderr;
        }
    }
    setbuf(g_log_file, NULL);
#endif

    g_dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(g_dpy) > 0) {
        ERR("could not create xcb connection\n");
#ifdef DEBUG
        fclose(g_log_file);
#endif
        exit(1);
    }

    cookies = xcb_ewmh_init_atoms(g_dpy, &g_ewmh);
    if (!xcb_ewmh_init_atoms_replies(&g_ewmh, cookies, NULL)) {
        ERR("could not set up ewmh\n");
#ifdef DEBUG
        fclose(g_log_file);
#endif
        exit(1);
    }

    init_font_drawing();

    set_font(NULL);

    signal(SIGALRM, alarm_handler);

    return 0;
}

void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* TODO: maybe free all resources to show we care? */
    deinit_font_drawing();
    xcb_disconnect(g_dpy);
#ifdef DEBUG
    fclose(g_log_file);
#endif
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
    xcb_configure_window(g_dpy, g_screen->notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH |
            XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_map_window(g_dpy, g_screen->notification_window);

    rect.x = 0;
    rect.y = 0;
    rect.width = tm.total_width;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    color.alpha = 0xff00;
    color.red = 0xff00;
    color.green = 0xff00;
    color.blue = 0xff00;
    draw_text(g_screen->notification_window, msg, msg_len, color, &rect,
            g_screen->stock_objects[STOCK_BLACK_PEN], WINDOW_PADDING / 2,
            tm.ascent + WINDOW_PADDING / 2);

    alarm(NOTIFICATION_DURATION);
}
