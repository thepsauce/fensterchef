#include "default_configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "program_options.h"
#include "render.h"
#include "window.h"
#include "x11_management.h"
#include "xalloc.h"

/* FENSTERCHEF main entry point. */
int main(int argc, char **argv)
{
    /* parse the program arguments */
    if (parse_program_arguments(argc, argv) != OK) {
        exit(EXIT_FAILURE);
    }

    /* initialize the X connection and X atoms */
    if (initialize_x11() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* try to take control of the window manager role and create utility windows
     */
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

    /* manage the windows that are already there */
    query_existing_windows();

    /* run all startup actions */
    LOG("running startup actions: %A\n",
            configuration.startup.number_of_actions,
            configuration.startup.actions);
    for (uint32_t i = 0; i < configuration.startup.number_of_actions; i++) {
        do_action(&configuration.startup.actions[i], focus_window);
    }

    /* do an inital synchronization */
    synchronize_with_server();
    synchronize_client_list();

    /* before entering the loop, flush all the initialization calls */
    xcb_flush(connection);

    is_fensterchef_running = true;
    /* run the main event loop */
    while (next_cycle() == OK) {
        /* nothing to do */
    }

    quit_fensterchef(is_fensterchef_running ? EXIT_FAILURE : EXIT_SUCCESS);
}
