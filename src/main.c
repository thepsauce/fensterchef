#include <stdlib.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "log.h"
#include "monitor.h"
#include "program_options.h"
#include "window_properties.h"
#include "x11_synchronize.h"

/* FENSTERCHEF main entry point. */
int main(int argc, char **argv)
{
    Fensterchef_home = getenv("HOME");
    if (Fensterchef_home == NULL) {
        Fensterchef_home = ".";
        LOG_ERROR("HOME is not set, using the arbritary %s\n",
                Fensterchef_home);
    }

    /* parse the program arguments */
    parse_program_arguments(argc, argv);

    LOG("parsed arguments, starting to log\n");
    LOG("welcome to " FENSTERCHEF_NAME " " FENSTERCHEF_VERSION "\n");
    LOG("the configuration file resides in %s\n",
            get_configuration_file());

    /* open connection to the X server */
    open_connection();

    /* initialize the X atoms */
    initialize_atoms();

    /* try to take control of the window manager role */
    take_control();

    /* set the signal handlers (very crucial because if we do not handle
     * `SIGALRM`, fensterchef quits)
     */
    initialize_signal_handlers();

    /* initialize randr if possible and the initial monitors with their
     * root frames
     */
    initialize_monitors();

    /* set the X properties on the root window */
    initialize_root_properties();

    /* manage the windows that are already there */
    query_existing_windows();

    /* configure the monitor frames before running the startup actions */
    reconfigure_monitor_frames();

    /* load the user configuration or the default configuration */
    reload_configuration();

    if (!Fensterchef_is_running) {
        LOG("startup interrupted by user configuration\n");
        quit_fensterchef(EXIT_FAILURE);
    }

    /* do an inital synchronization */
    synchronize_with_server(SYNCHRONIZE_ALL);

    /* before entering the loop, flush all the initialization calls */
    XFlush(display);

    /* run the main event loop */
    while (next_cycle() == OK) {
        /* nothing to do */
    }

    quit_fensterchef(Fensterchef_is_running ? EXIT_FAILURE : EXIT_SUCCESS);
}
