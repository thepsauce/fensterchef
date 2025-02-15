#include <stdlib.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "render.h"
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
        return ERROR;
    }

    /* initialize the key symbol table */
    if (initialize_keymap() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the @screen with graphical stock objects and utility windows */
    if (initialize_screen(screen_number) != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* try to initialize randr and set @screen->monitor */
    initialize_monitors();

    /* log the screen information */
    log_screen();

    /* initialize graphical stock objects used for rendering */
    if (initialize_renderer() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* load the default configuration and the user configuration, this also
     * initializes the keybinds and font
     */
    load_default_configuration();
    reload_user_configuration();

    /* set the X properties on the root window */
    synchronize_all_root_properties();

    /* select the first frame */
    focus_frame = get_primary_monitor()->frame;

    /* run the main event loop */
    is_fensterchef_running = true;
    while (is_fensterchef_running) {
        event = xcb_wait_for_event(connection);
        if (event == NULL) {
            quit_fensterchef(EXIT_FAILURE);
        }

        handle_event(event);

        if (reload_requested) {
            reload_user_configuration();
            reload_requested = false;
        }

        free(event);

        /* flush after every event so all changes are reflected */
        xcb_flush(connection);
    }

    quit_fensterchef(EXIT_SUCCESS);
}
