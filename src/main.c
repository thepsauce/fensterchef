#include <stdlib.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keybind.h"
#include "keymap.h"
#include "log.h"
#include "root_properties.h"
#include "screen.h"
#include "xalloc.h"

/* FENSTERCHEF main entry point. */
int main(void)
{
    int screen_number;
    xcb_generic_event_t *event;

    /* initialize logging, the xcb/ewmh connection and font drawing */
    if (initialize_fensterchef(&screen_number) != 0) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the key symbol table */
    if (initialize_keymap() != 0) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the @screen with graphical stock objects and utility windows */
    if (initialize_screen(screen_number) != 0) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* try to initialize randr and set @screen->monitor */
    initialize_monitors();

    /* log the screen information */
    log_screen();

    /* load the default configuration and the user configuration, this also
     * initializes the keybinds and font
     */
    load_default_configuration();
    reload_user_configuration();

    /* set the properties on the root window */
    synchronize_all_root_properties();

    /* select the first frame */
    focus_frame = get_primary_monitor()->frame;

    /* run the main event loop */
    is_fensterchef_running = true;
    while (is_fensterchef_running) {
        if (xcb_connection_has_error(connection) > 0) {
            quit_fensterchef(EXIT_FAILURE);
        }
        event = xcb_wait_for_event(connection);
        if (event != NULL) {
            handle_event(event);
            free(event);
            xcb_flush(connection);
        }
    }

    quit_fensterchef(EXIT_SUCCESS);
}
