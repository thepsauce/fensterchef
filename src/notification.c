#include <unistd.h>

#include "configuration/default.h"
#include "log.h"
#include "notification.h"
#include "x11_management.h"

/* notification window */
struct notification Notification;

/* Initialize the notification window. */
static int initialize_notification(void)
{
    XSetWindowAttributes attributes;

    if (Notification.client.id == None) {
        Notification.client.x = -1;
        Notification.client.y = -1;
        Notification.client.width = 1;
        Notification.client.height = 1;
        Notification.client.border_width =
            default_configuration.notification.border_size;
        Notification.client.border =
            default_configuration.notification.border_color;
        Notification.client.background =
            default_configuration.notification.background;
        attributes.border_pixel = Notification.client.border;
        attributes.backing_pixel = Notification.client.background;
        /* indicate to not manage the window */
        attributes.override_redirect = True;
        Notification.client.id = XCreateWindow(display,
                    DefaultRootWindow(display), Notification.client.x,
                    Notification.client.y, Notification.client.width,
                    Notification.client.height,
                    Notification.client.border_width, CopyFromParent,
                    InputOutput, (Visual*) CopyFromParent,
                    CWBorderPixel | CWBackPixel | CWOverrideRedirect,
                    &attributes);

        if (Notification.client.id == None) {
            LOG_ERROR("failed creating notification window\n");
            return ERROR;
        }

        const char *const notification_name = "[fensterchef] notification";
        XStoreName(display, Notification.client.id, notification_name);
    }

    /* create an XftDraw object if not done already */
    if (Notification.xft_draw == NULL) {
        Notification.xft_draw = XftDrawCreate(display, Notification.client.id,
                DefaultVisual(display, DefaultScreen(display)),
                DefaultColormap(display, DefaultScreen(display)));

        if (Notification.xft_draw == NULL) {
            LOG_ERROR("could not create XftDraw for the notification window\n");
            return ERROR;
        }
    }
    return OK;
}

/* Show the notification window with given message at given coordinates for
 * a duration in seconds specified in the configuration.
 */
void set_notification(const utf8_t *message, int x, int y)
{
    XftColor background, foreground;
    FcChar32 *glyphs;
    int glyph_count;
    Text text;

    if (configuration.notification.duration == 0) {
        return;
    }

    if (get_xft_colors(&background, &foreground) == ERROR) {
        return;
    }

    /* initialize the notification window if not done already */
    if (initialize_notification() == ERROR) {
        return;
    }

    glyphs = get_glyphs(message, -1, &glyph_count);

    initialize_text(&text, glyphs, glyph_count);

    /* add the padding */
    text.x += configuration.notification.padding / 2;
    text.y += configuration.notification.padding / 2;
    text.width += configuration.notification.padding;
    text.height += configuration.notification.padding;

    /* center the text window */
    x -= text.width / 2;
    y -= text.height / 2;

    /* attempt to put the window fully in bounds */
    const unsigned
        display_width = DisplayWidth(display, DefaultScreen(display)),
        display_height = DisplayHeight(display, DefaultScreen(display));
    if (x < 0) {
        x = 0;
    } else if ((unsigned) x + text.width +
            configuration.notification.border_size * 2 >= display_width) {
        x = display_width - text.width -
            configuration.notification.border_size * 2;
    }
    if (y < 0) {
        y = 0;
    } else if ((unsigned) y + text.height +
            configuration.notification.border_size * 2 >= display_height) {
        y = display_height - text.height -
            configuration.notification.border_size * 2;
    }

    /* change border color and size of the notification window */
    change_client_attributes(&Notification.client,
            configuration.notification.background,
            configuration.notification.border_color);

    /* set the window size, position and set it above */
    configure_client(&Notification.client, x, y, text.width,
            text.height, configuration.notification.border_size);

    /* put the window at the top */
    XRaiseWindow(display, Notification.client.id);

    /* show the window */
    map_client(&Notification.client);

    /* draw background and text */
    XftDrawRect(Notification.xft_draw, &background,
            0, 0, text.width, text.height);
    draw_text(Notification.xft_draw, &foreground,
            text.x, text.y, &text);

    clear_text(&text);

    LOG_DEBUG("showed notification: %s at %R with offset: %P\n",
            message,
            Notification.client.x, Notification.client.y,
            Notification.client.width, Notification.client.height,
            text.x, text.y);

    /* set an alarm to trigger after the specified seconds */
    alarm(configuration.notification.duration);
}
