#include "fensterchef.h"
#include "frame.h"
#include "log.h"

int main(void)
{
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
        handle_next_event();
        xcb_flush(g_dpy);
    }

    xcb_disconnect(g_dpy);
    return 0;
}
