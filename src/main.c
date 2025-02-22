#include <stdlib.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "root_properties.h"
#include "x11_management.h"
#include "xalloc.h"

/* FENSTERCHEF main entry point. */
int main(void)
{
    /* initialize the X connection, X atoms and create utility windows */
    if (initialize_x11() != OK) {
        return ERROR;
    }

    /* try to take control of the windows and start managing */
    if (take_control() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the key symbol table */
    if (initialize_keymap() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* try to initialize randr */
    initialize_monitors();

    /* log the screen information */
    log_screen();

    /* initialize graphical stock objects used for rendering */
    if (initialize_renderer() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* load the default configuration and the user configuration, this also
     * initializes the keybindings and font
     */
    load_default_configuration();
    reload_user_configuration();

    /* set the signal handlers */
    if (initialize_signal_handlers() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* set the X properties on the root window */
    synchronize_all_root_properties();

    /* select the first frame */
    focus_frame = get_primary_monitor()->frame;

    /* run the main event loop */
    is_fensterchef_running = true;
    while (next_cycle(NULL) == OK) {
        /* nothing to do */
    }

    quit_fensterchef(EXIT_SUCCESS);
}
