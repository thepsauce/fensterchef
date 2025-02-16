#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "render.h"
#include "root_properties.h"
#include "screen.h"
#include "x.h"
#include "xalloc.h"

/* Handle an incoming alarm. */
static void alarm_handler(int signal)
{
    (void) signal;
    LOG("triggered alarm: hiding notification window\n");
    /* hide the notification window */
    xcb_unmap_window(connection, screen->notification_window);
    /* flush is needed because the only place we flush is after receiving an
     * event so the changes would not be reflected until then
     */
    xcb_flush(connection);
}

/* FENSTERCHEF main entry point. */
int main(void)
{
    xcb_generic_event_t *event;

    /* initialize logging, the xcb/ewmh connection and font drawing */
    if (x_initialize() != 0) {
        return ERROR;
    }

    /* create a signal handle for the alarm signal, this is triggered when the
     * alarm created by `alarm()` expires
     */
    signal(SIGALRM, alarm_handler);

    /* initialize the key symbol table */
    if (initialize_keymap() != OK) {
        quit_fensterchef(EXIT_FAILURE);
    }

    /* initialize the @screen with graphical stock objects and utility windows */
    if (initialize_screen() != OK) {
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
