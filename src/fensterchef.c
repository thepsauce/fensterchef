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

#include "configuration.h"
#include "fensterchef.h"
#include "log.h"
#include "render.h"
#include "screen.h"
#include "utility.h"

/* xcb server connection */
xcb_connection_t        *connection;

/* ewmh (extended window manager hints) information */
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
    /* hide the notification window */
    xcb_unmap_window(connection, screen->notification_window);
    /* flush is needed because the only place we flush is after receiving an
     * event so the changes would not be reflected until then
     */
    xcb_flush(connection);
}

/* Initialize logging, the xcb/ewmh connection and font drawing. */
int initialize_fensterchef(int *screen_number)
{
    xcb_intern_atom_cookie_t *atom_cookies;
    xcb_generic_error_t *error;

    /* read the DISPLAY environment variable to determine the display to
     * attach to; if the DISPLAY variable is in the form :X.Y then X is the
     * display number and Y the screen number which is stored in `screen_number
     */
    connection = xcb_connect(NULL, screen_number);
    /* standard way to check if a connection failed */
    if (xcb_connection_has_error(connection) > 0) {
        LOG_ERROR(NULL, "could not create xcb connection");
        return ERROR;
    }

    /* intern the atoms into the xcb server, they are stored in an array of
     * cookies
     */
    atom_cookies = xcb_ewmh_init_atoms(connection, &ewmh);
    /* get the reply for all cookies and set the ewmh atoms to the values the
     * xcb server assigned for us
     */
    if (!xcb_ewmh_init_atoms_replies(&ewmh, atom_cookies, &error)) {
        LOG_ERROR(error, "could not set up ewmh");
        return ERROR;
    }

    /* initialize freetype and fontconfig for drawing fonts */
    (void) initialize_font_drawing();

    /* create a signal handle for the alarm signal, this is triggered when the
     * alarm created by `alarm()` expires
     */
    signal(SIGALRM, alarm_handler);

    return OK;
}

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* TODO: maybe free all resources to show we care? */
    deinitialize_renderer();
    xcb_disconnect(connection);
    exit(exit_code);
}

/* Show the notification window with given message at given coordinates. */
void set_notification(const uint8_t *message, int32_t x, int32_t y)
{
    size_t              message_length;
    struct text_measure measure;
    xcb_rectangle_t     rectangle;
    xcb_render_color_t  color;

    if (configuration.notification.duration == 0) {
        return;
    }

    /* measure the text for centering the text */
    message_length = strlen((char*) message);
    measure_text(message, message_length, &measure);

    measure.total_width += configuration.notification.padding;

    x -= measure.total_width / 2;
    y -= (measure.ascent - measure.descent +
            configuration.notification.padding) / 2;

    /* set the window size and position */
    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = measure.total_width;
    general_values[3] = measure.ascent - measure.descent +
        configuration.notification.padding;
    general_values[4] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, screen->notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, general_values);
    /* show the window */
    xcb_map_window(connection, screen->notification_window);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = measure.total_width;
    rectangle.height = measure.ascent - measure.descent +
        configuration.notification.padding;
    convert_color_to_xcb_color(&color, configuration.notification.background);
    draw_text(screen->notification_window, message, message_length, color,
            &rectangle, stock_objects[STOCK_BLACK_PEN],
            configuration.notification.padding / 2,
            measure.ascent + configuration.notification.padding / 2);

    /* set an alarm to trigger after @configuration.notification.duration */
    alarm(configuration.notification.duration);
}
