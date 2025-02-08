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
xcb_connection_t        *connection;

/* ewmh information */
xcb_ewmh_connection_t   ewmh;

/* true while the window manager is running */
bool                    is_fensterchef_running;

/* general purpose values */
uint32_t                general_values[7];

/* Handle an incoming alarm. */
static void alarm_handler(int signal)
{
    (void) signal;
    LOG("triggered alarm: hiding notification window\n");
    xcb_unmap_window(connection, screen->notification_window);
    xcb_flush(connection);
}

/* Initialize fensterchef core data. */
int init_fensterchef(int *screen_number)
{
    xcb_intern_atom_cookie_t *atom_cookies;

#ifdef DEBUG
    if ((void*) LOG_FILE == (void*) stderr) {
        log_file = stderr;
    } else {
        log_file = fopen(LOG_FILE, "w");
        if (log_file == NULL) {
            log_file = stderr;
        }
    }
    setbuf(log_file, NULL);
#endif

    connection = xcb_connect(NULL, screen_number);
    if (xcb_connection_has_error(connection) > 0) {
        ERR("could not create xcb connection\n");
#ifdef DEBUG
        fclose(log_file);
#endif
        exit(1);
    }

    atom_cookies = xcb_ewmh_init_atoms(connection, &ewmh);
    if (!xcb_ewmh_init_atoms_replies(&ewmh, atom_cookies, NULL)) {
        ERR("could not set up ewmh\n");
#ifdef DEBUG
        fclose(log_file);
#endif
        exit(1);
    }

    init_font_drawing();

    signal(SIGALRM, alarm_handler);

    return 0;
}

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* TODO: maybe free all resources to show we care? */
    deinit_font_drawing();
    xcb_disconnect(connection);
#ifdef DEBUG
    fclose(log_file);
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

    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = tm.total_width;
    general_values[3] = tm.ascent - tm.descent + WINDOW_PADDING;
    general_values[4] = 1;
    general_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, screen->notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH |
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    xcb_map_window(connection, screen->notification_window);

    rect.x = 0;
    rect.y = 0;
    rect.width = tm.total_width;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    color.alpha = 0xff00;
    color.red = 0xff00;
    color.green = 0xff00;
    color.blue = 0xff00;
    draw_text(screen->notification_window, msg, msg_len, color, &rect,
            screen->stock_objects[STOCK_BLACK_PEN], WINDOW_PADDING / 2,
            tm.ascent + WINDOW_PADDING / 2);

    alarm(NOTIFICATION_DURATION);
}
