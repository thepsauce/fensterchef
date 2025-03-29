#include "configuration/default.h"
#include "cursor.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "program_options.h"
#include "render.h"
#include "resources.h"
#include "window.h"
#include "window_properties.h"
#include "x11_management.h"
#include "xalloc.h"

/* FENSTERCHEF main entry point. */
int main(int argc, char **argv)
{
    Fensterchef_home = getenv("HOME");
    if (Fensterchef_home == NULL) {
        fprintf(stderr, "to run fensterchef, you must set HOME\n");
        exit(EXIT_FAILURE);
    }

    /* either use XDG_CONFIG_HOME as the configuration directory or ~/.config */
    {
        const char *xdg_config_home;

        xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home == NULL) {
            Fensterchef_configuration = xasprintf("%s/.config/"
                        FENSTERCHEF_CONFIGURATION,
                    Fensterchef_home);
        } else {
            Fensterchef_configuration = xasprintf("%s/"
                        FENSTERCHEF_CONFIGURATION,
                    xdg_config_home);
        }
    }

    /* parse the program arguments */
    if (parse_program_arguments(argc, argv) != OK) {
        exit(EXIT_FAILURE);
    }

    LOG("parsed arguments, starting to log\n");
    LOG("welcome to " FENSTERCHEF_NAME " " FENSTERCHEF_VERSION "\n");
    LOG("the configuration file may reside in %s\n", Fensterchef_configuration);

    /* initialize the X connection */
    if (initialize_x11() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the X atoms */
    if (initialize_atoms() != OK) {
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

    /* load the user configuration or the default configuration; this also
     * initializes the bindings and font
     */
    {
        struct configuration configuration;

        if (load_configuration(Fensterchef_configuration, &configuration,
                    true) != OK) {
            load_default_configuration();
        } else {
            set_configuration(&configuration);
        }
    }

    /* manage the windows that are already there */
    query_existing_windows();

    /* configure the monitor frames before running the startup actions */
    reconfigure_monitor_frames();

    Fensterchef_is_running = true;

    /* run the startup expression */
    LOG("running startup expression: %A\n", &configuration.startup.expression);
    evaluate_expression(&configuration.startup.expression, NULL);

    if (!Fensterchef_is_running) {
        LOG("startup interrupted by user configuration\n");
        quit_fensterchef(EXIT_SUCCESS);
    }

    /* do an inital synchronization */
    synchronize_with_server();
    synchronize_client_list();

    /* before entering the loop, flush all the initialization calls */
    xcb_flush(connection);

    /* run the main event loop */
    while (next_cycle() == OK) {
        /* nothing to do */
    }

    quit_fensterchef(Fensterchef_is_running ? EXIT_FAILURE : EXIT_SUCCESS);
}
