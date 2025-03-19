#include <unistd.h> // alarm()

#include "configuration.h"
#include "fensterchef.h"
#include "log.h"
#include "render.h"
#include "x11_management.h"

/* the home directory */
const char *Fensterchef_home;

/* true while the window manager is running */
bool Fensterchef_is_running;

/* the path of the configuration file */
const char *Fensterchef_configuration = FENSTERCHEF_CONFIGURATION;

/* Spawn a window that has the `FENSTERCHEF_COMMAND` property. */
void run_external_command(const char *command)
{
    xcb_connection_t *connection;
    int screen_number;
    int connection_error;

    const xcb_setup_t *setup;
    xcb_screen_iterator_t i;
    xcb_screen_t *screen;

    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom;
    xcb_atom_t command_atom;

    xcb_window_t window;
    xcb_generic_event_t *event;

    connection = xcb_connect(NULL, &screen_number);
    connection_error = xcb_connection_has_error(connection);
    if (connection_error > 0) {
        fprintf(stderr, "fensterchef command: "
                    "could not connect to the X server\n");
        return;
    }

    setup = xcb_get_setup(connection);

    screen = NULL;
    /* iterator over all screens to find the one with the screen number */
    for(i = xcb_setup_roots_iterator(setup); i.rem > 0; xcb_screen_next(&i)) {
        if (screen_number == 0) {
            screen = i.data;
            break;
        }
        screen_number--;
    }

    if (screen == NULL) {
        fprintf(stderr, "fensterchef command: "
                    "screen %d does not exist\n", screen_number);
        return;
    }

    window = xcb_generate_id(connection);

    atom_cookie = xcb_intern_atom(connection, false,
            strlen("FENSTERCHEF_COMMAND"), "FENSTERCHEF_COMMAND");

    general_values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window,
            screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
            XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, general_values);

    atom = xcb_intern_atom_reply(connection, atom_cookie, NULL);
    if (atom == NULL) {
        fprintf(stderr, "fensterchef command: could not intern atom %s\n",
                "FENSTERCHEF_COMMAND");
        xcb_disconnect(connection);
        return;
    }
    command_atom = atom->atom;
    free(atom);

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
            command_atom, XCB_ATOM_STRING, 8, strlen(command), command);

    xcb_map_window(connection, window);

    fprintf(stderr, "fensterchef command: command was dispatched, "
                "waiting until execution...\n");

    xcb_flush(connection);

    /* wait until the property gets removed or an error occurs */
    while (event = xcb_wait_for_event(connection), event != NULL) {
        if (event->response_type == 0) {
            fprintf(stderr, "fensterchef command: an error occured\n");
            break;
        }
        if (event->response_type == XCB_PROPERTY_NOTIFY) {
            xcb_property_notify_event_t *notify;

            notify = (xcb_property_notify_event_t*) event;
            /* if the property gets removed, it means the command was executed
             */
            if (notify->atom == command_atom &&
                    notify->state == XCB_PROPERTY_DELETE) {
                break;
            }
        }
    }

    xcb_disconnect(connection);
}

/* Close the connection to the X server and exit the program with given exit
 * code.
 */
void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    xcb_disconnect(connection);
    exit(exit_code);
}

/* Show the notification window with given message at given coordinates. */
void set_notification(const utf8_t *message, int32_t x, int32_t y)
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

    /* set the window size, position and set it above */
    configure_client(&notification,
            x, y,
            measure.total_width,
            measure.ascent - measure.descent +
                configuration.notification.padding,
            notification.border_width);

    general_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, notification.id,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* show the window */
    map_client(&notification);

    /* render the notification on the window */
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = measure.total_width;
    rectangle.height = measure.ascent - measure.descent +
        configuration.notification.padding;
    convert_color_to_xcb_color(&color, configuration.notification.background);
    draw_text(notification.id, message, message_length, color,
            &rectangle, stock_objects[STOCK_BLACK_PEN],
            configuration.notification.padding / 2,
            measure.ascent + configuration.notification.padding / 2);

    /* set an alarm to trigger after @configuration.notification.duration */
    alarm(configuration.notification.duration);
}
