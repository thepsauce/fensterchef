#include <unistd.h>

#include "configuration.h"
#include "fensterchef.h"
#include "log.h"
#include "render.h"
#include "x11_management.h"

/* true while the window manager is running */
bool is_fensterchef_running;

/* the path of the configuration file */
const char *fensterchef_configuration = FENSTERCHEF_CONFIGURATION;

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* TODO: maybe free all resources to show we care? that would include:
     * - frames
     * - windows
     * - monitors
     * - configuration
     * - render cache
     */
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

    /* set the window size, position and set it above */
    configure_client(&notification,
            x,
            y,
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
