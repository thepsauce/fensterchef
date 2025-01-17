#include <stdlib.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"

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
