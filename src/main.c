#include "default_configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "x11_management.h"
#include "xalloc.h"

/* FENSTERCHEF main entry point. */
int main(void)
{
    /* initialize the X connection, X atoms and create utility windows */
    if (initialize_x11() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* try to take control of the windows and start managing */
    if (take_control() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the key symbol table */
    if (initialize_keymap() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize graphical stock objects used for rendering */
    if (initialize_renderer() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* set the signal handlers */
    if (initialize_signal_handlers() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize randr if possible and the initial frames */
    initialize_monitors();

    /* set the X properties on the root window */
    initialize_root_properties();

    /* load the default configuration and the user configuration, this also
     * initializes the bindings and font
     */
    load_default_configuration();
    reload_user_configuration();

    /* before entering the loop, flush all the initialization calls */
    xcb_flush(connection);

    is_fensterchef_running = true;
    /* run the main event loop */
    while (next_cycle(NULL) == OK) {
        /* nothing to do */
    }

    quit_fensterchef(is_fensterchef_running ? EXIT_FAILURE : EXIT_SUCCESS);
}
