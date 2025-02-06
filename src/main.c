#include <stdlib.h>

#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keybind.h"
#include "keymap.h"
#include "log.h"
#include "screen.h"
#include "xalloc.h"

/* FENSTERCHEF
 *
 * First, fensterchef is initialized, see init_fensterchef() for details and the
 * keyboard is setup (see init_keymap())
 *
 * Then the main event loop runs, waiting for every xcb event and letting them
 * be handled by handle_event().
 */
int main(void)
{
    xcb_generic_event_t *event;

    if (init_fensterchef() != 0 ||
            init_keymap() != 0 ||
            /* TODO: take user chosen screen. */
            init_screen(g_ewmh.screens[0]) != 0) {
        quit_fensterchef(EXIT_FAILURE);
    }

    init_monitors();

    log_screen();

    g_cur_frame = get_primary_monitor()->frame;

    g_running = true;
    while (g_running) {
        if (xcb_connection_has_error(g_dpy) > 0) {
            quit_fensterchef(EXIT_FAILURE);
        }
        event = xcb_wait_for_event(g_dpy);
        if (event != NULL) {
            handle_event(event);
            free(event);
            xcb_flush(g_dpy);
        }
    }

    quit_fensterchef(EXIT_SUCCESS);
}
