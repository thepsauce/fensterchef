#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "notify.h"

int main(void)
{
    g_log_file = fopen("/tmp/fensterchef-log.txt", "w");
    if (g_log_file == NULL) {
        g_log_file = stderr;
    }
    init_connection();
    init_screens();
    if (setup_keys() != 0 || init_notification() != 0) {
        close_connection();
        return 1;
    }
    if (g_screen_count > 0) {
        /* take the first screen */
        g_screen_no = 0;
        create_frame(NULL, 0, 0, g_screens[g_screen_no]->width_in_pixels,
                g_screens[g_screen_no]->height_in_pixels);
        g_running = !take_control();
    }

    while (g_running) {
        handle_next_event();
        xcb_flush(g_dpy);
    }

    close_connection();
    return 0;
}
