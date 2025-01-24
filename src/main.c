#include <stdlib.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"

/* FENSTERCHEF
 *
 * First, fensterchef is initialized, see init_fensterchef() for details and the
 * keyboard is setup (see setup_keys()).
 *
 * Then the main event loop runs, waiting for every xcb event and letting them
 * be handled by handle_event().
 */
int main(void)
{
    xcb_generic_event_t *event;

    if (init_fensterchef() != 0 ||
            setup_keys() != 0) {
        quit_fensterchef(1);
    }

    g_running = 1;
    while (g_running) {
        if (xcb_connection_has_error(g_dpy) > 0) {
            quit_fensterchef(1);
        }
        event = xcb_wait_for_event(g_dpy);
        if (event != NULL) {
            handle_event(event);
            free(event);
            xcb_flush(g_dpy);
        }
    }

    quit_fensterchef(0);
}
