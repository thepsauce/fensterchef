#include "log.h"
#include "window.h"
#include "x11_synchronize.h"

/* Remove @window from the Z linked list. */
void unlink_window_from_z_list(FcWindow *window)
{
    if (window->below != NULL) {
        window->below->above = window->above;
    } else {
        Window_bottom = window->above;
    }

    if (window->above != NULL) {
        window->above->below = window->below;
    } else {
        Window_top = window->below;
    }

    window->above = NULL;
    window->below = NULL;
}

/* Links the window into the z linked list at a specific place. */
void link_window_above_in_z_list(FcWindow *window, FcWindow *below)
{
    /* link onto the bottom of the Z linked list */
    if (below == NULL) {
        if (Window_bottom != NULL) {
            Window_bottom->below = window;
            window->above = Window_bottom;
        } else {
            Window_top = window;
        }
        Window_bottom = window;
    /* link it above `below` */
    } else {
        window->below = below;
        window->above = below->above;
        if (below->above != NULL) {
            below->above->below = window;
        } else {
            Window_top = window;
        }
        below->above = window;
    }
}

/* Remove @window from the Z server linked list. */
void unlink_window_from_z_server_list(FcWindow *window)
{
    if (window->server_below != NULL) {
        window->server_below->server_above = window->server_above;
    }

    if (window->server_above != NULL) {
        window->server_above->server_below = window->server_below;
    } else {
        Window_server_top = window->server_below;
    }

    window->server_above = NULL;
    window->server_below = NULL;
}

/* Links the window into the Z server linked list at a specific place. */
void link_window_above_in_z_server_list(FcWindow *window, FcWindow *below)
{
    window->server_below = below;
    window->server_above = below->server_above;
    if (below->server_above != NULL) {
        below->server_above->server_below = window;
    } else {
        Window_server_top = window;
    }
    below->server_above = window;
}

/* Put the window on the best suited Z stack position. */
void update_window_layer(FcWindow *window)
{
    FcWindow *below = NULL;

    unlink_window_from_z_list(window);

    switch (window->state.mode) {
    /* If there are desktop windows, put the window on top of all desktop
     * windows.  Otherwise put it at the bottom.
     */
    case WINDOW_MODE_TILING:
        if (Window_bottom != NULL &&
                Window_bottom->state.mode == WINDOW_MODE_DESKTOP) {
            below = Window_bottom;
            while (below->above != NULL &&
                    below->state.mode == WINDOW_MODE_DESKTOP) {
                below = below->above;
            }
        }
        break;

    /* put the window at the top */
    case WINDOW_MODE_FLOATING:
    case WINDOW_MODE_FULLSCREEN:
    case WINDOW_MODE_DOCK:
        below = Window_top;
        break;

    /* put the window at the bottom */
    case WINDOW_MODE_DESKTOP:
        /* below = NULL */
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        return;
    }

    link_window_above_in_z_list(window, below);

    /* put windows that are transient for this window above it */
    for (below = window->below; below != NULL; below = below->below) {
        if (below->transient_for == window->client.id) {
            unlink_window_from_z_list(below);
            /* note the reverse order here, it is important */
            link_window_above_in_z_list(below, window);
            below = window;
        }
    }
}

/* Synchronize the window stacking order with the server. */
void synchronize_window_stacking_order(void)
{
    FcWindow *window, *server_window;
    XWindowChanges changes;

    window = Window_top;
    server_window = Window_server_top;
    /* go through both lists from top to bottom at the same time */
    for (; window != NULL && server_window != NULL; window = window->below) {
        if (server_window == window) {
            server_window = server_window->server_below;
        } else {
            unlink_window_from_z_server_list(window);
            link_window_above_in_z_server_list(window, server_window);

            changes.stack_mode = Above;
            changes.sibling = server_window->client.id;
            XConfigureWindow(display, window->client.id,
                    CWStackMode | CWSibling, &changes);
            LOG("putting window %W above %W\n",
                    window, server_window);
        }
    }
}
