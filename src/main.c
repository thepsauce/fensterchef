#include <stdlib.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"

int main(void)
{
    xcb_generic_event_t *event;

    if ((void*) LOG_FILE == (void*) stderr) {
        g_log_file = stderr;
    } else {
        g_log_file = fopen(LOG_FILE, "w");
        if (g_log_file == NULL) {
            g_log_file = stderr;
        }
    }

    if (init_fensterchef() != 0 ||
            setup_keys() != 0) {
        xcb_disconnect(g_dpy);
        return 1;
    }

    g_running = 1;
    while (g_running) {
        if (xcb_connection_has_error(g_dpy) > 0) {
            /* quit because the connection is broken */
            break;
        }
        event = xcb_wait_for_event(g_dpy);
        if (event != NULL) {
            handle_event(event);
            free(event);
            xcb_flush(g_dpy);
        }
    }

    xcb_disconnect(g_dpy);
    return 0;
}
