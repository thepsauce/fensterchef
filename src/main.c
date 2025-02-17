#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

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

/* signals whether the alarm signal was received */
volatile sig_atomic_t timer_expired_signal;

/* Handle an incoming alarm. */
static void alarm_handler(int signal)
{
    (void) signal;
    timer_expired_signal = true;
}

/* FENSTERCHEF main entry point. */
int main(void)
{
    struct sigaction action;
    fd_set set;
    xcb_generic_event_t *event;
    int x_file_descriptor;

    /* initialize the X connection, X atoms and create utility windows */
    if (x_initialize() != OK) {
        return ERROR;
    }

    /* try to take control of the windows and start managing */
    if (x_take_control() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* create a signal handle for the alarm signal, this is triggered when the
     * alarm created by `alarm()` expires
     */
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        LOG_ERROR(NULL, "could not create alarm handler\n");
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

    /* set the X properties on the root window */
    synchronize_all_root_properties();

    /* select the first frame */
    focus_frame = get_primary_monitor()->frame;

    /* run the main event loop */
    is_fensterchef_running = true;
    FD_ZERO(&set);
    x_file_descriptor = xcb_get_file_descriptor(connection);
    while (is_fensterchef_running &&
            xcb_connection_has_error(connection) == 0) {
        FD_SET(x_file_descriptor, &set);

        /* using select here is key: select will block until data on the file
         * descriptor for the X connection arrives; when a signal is received,
         * `select()` will however also unblock and return -1
         */
        if (select(x_file_descriptor + 1, &set, NULL, NULL, NULL) == 1) {
            /* handle all received events */
            while (event = xcb_poll_for_event(connection), event != NULL) {
                handle_event(event);

                if (reload_requested) {
                    reload_user_configuration();
                    reload_requested = false;
                }

                free(event);
            }
        }

        if (timer_expired_signal) {
            LOG("triggered alarm: hiding notification window\n");
            /* hide the notification window */
            xcb_unmap_window(connection, notification_window);
            timer_expired_signal = false;
        }

        /* flush after every event so all changes are reflected */
        xcb_flush(connection);
    }

    quit_fensterchef(EXIT_SUCCESS);
}
