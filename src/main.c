#include "fensterchef.h"

int main(void)
{
    init_connection();
    init_screens();
    grab_keys();
    if (g_screen_count > 0) {
        /* take the first screen */
        g_screen_no = 0;
        g_running = !take_control();
    }

    while (g_running) {
        handle_event();
    }

    close_connection();
    return 0;
}
