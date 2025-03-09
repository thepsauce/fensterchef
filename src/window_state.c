#include <inttypes.h>

#include "configuration.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* The whole purpose of this file is to handle window state changes
 * This includes visibility and window mode.
 */

/* Check if @window should have a border. */
bool has_window_border(Window *window)
{
    /* tiling windows always have a border */
    if (window->state.mode == WINDOW_MODE_TILING) {
        return true;
    }
    /* fullscreen and dock windows have no border */
    if (window->state.mode != WINDOW_MODE_FLOATING) {
        return false;
    }
    /* if the window has borders itself (not set by the window manager) */
    if ((window->motif_wm_hints.flags & MOTIF_WM_HINTS_DECORATIONS)) {
        return false;
    }
    return true;
}

/* Set the window size and position according to the size hints. */
static void configure_floating_size(Window *window)
{
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    monitor = get_monitor_from_rectangle_or_primary(window->x, window->y,
            window->width, window->height);

    /* if the window never had a floating size, use the size hints to get a size
     * that the window prefers
     */
    if (window->floating.width == 0) {
        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
            width = window->size_hints.width;
            height = window->size_hints.height;
        } else {
            width = monitor->width * 2 / 3;
            height = monitor->height * 2 / 3;
        }

        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            width = MAX(width, (uint32_t) window->size_hints.min_width);
            height = MAX(height, (uint32_t) window->size_hints.min_height);
        }

        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
            width = MIN(width, (uint32_t) window->size_hints.max_width);
            height = MIN(height, (uint32_t) window->size_hints.max_height);
        }

        /* center the window */
        x = monitor->x + (int32_t) (monitor->width - width) / 2;
        y = monitor->y + (int32_t) (monitor->height - height) / 2;
    } else {
        width = window->floating.width;
        height = window->floating.height;
        /* if the window would still be in the monitor is was moved to,
         * restore the position, otherwise center the window
         */
        if (get_monitor_from_rectangle(window->floating.x,
                    window->floating.y, window->floating.width,
                    window->floating.height) == monitor) {
            x = window->floating.x;
            y = window->floating.y;
        } else {
            x = monitor->x + (int32_t) (monitor->width - width) / 2;
            y = monitor->y + (int32_t) (monitor->height - height) / 2;
        }
    }

    set_window_size(window, x, y, width, height);
}

/* Sets the position and size of the window to fullscreen. */
static void configure_fullscreen_size(Window *window)
{
    Monitor *monitor;

    if (window->fullscreen_monitors.top != window->fullscreen_monitors.bottom) {
        set_window_size(window, window->fullscreen_monitors.left,
                window->fullscreen_monitors.top,
                window->fullscreen_monitors.right -
                    window->fullscreen_monitors.left,
                window->fullscreen_monitors.bottom -
                    window->fullscreen_monitors.left);
    } else {
        monitor = get_monitor_from_rectangle_or_primary(window->x,
                window->y, window->width, window->height);
        set_window_size(window, monitor->x, monitor->y,
                monitor->width, monitor->height);
    }
}

/* Sets the position and size of the window to a dock window. */
static void configure_dock_size(Window *window)
{
    const uint32_t both_hints = XCB_ICCCM_SIZE_HINT_P_POSITION |
        XCB_ICCCM_SIZE_HINT_P_SIZE;
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    /* check if the window has both position and size defined */
    if ((window->size_hints.flags & both_hints) == both_hints) {
        x = window->size_hints.x;
        y = window->size_hints.y;
        width = window->size_hints.width;
        height = window->size_hints.height;
        monitor = get_monitor_from_rectangle_or_primary(window->x, window->y,
                window->width, window->height);
    } else {
        monitor = get_monitor_from_rectangle_or_primary(window->x, window->y,
                1, 1);

        /* if the window does not specify a size itself, then do it based on the
         * strut the window defines, reasoning is that when the window wants to
         * occupy screen space, then it should be within that occupied space
         */
        if (window->strut.reserved.left != 0) {
            x = monitor->x;
            y = window->strut.left_start_y;
            width = window->strut.reserved.left;
            height = window->strut.left_end_y - window->strut.left_start_y + 1;
        } else if (window->strut.reserved.top != 0) {
            x = window->strut.top_start_x;
            y = monitor->y;
            width = window->strut.top_end_x - window->strut.top_start_x + 1;
            height = window->strut.reserved.top;
        } else if (window->strut.reserved.right != 0) {
            x = monitor->x + monitor->width - window->strut.reserved.right;
            y = window->strut.right_start_y;
            width = window->strut.reserved.right;
            height = window->strut.right_end_y -
                window->strut.right_start_y + 1;
        } else if (window->strut.reserved.bottom != 0) {
            x = window->strut.bottom_start_x;
            y = monitor->y + monitor->height - window->strut.reserved.bottom;
            width = window->strut.bottom_end_x -
                window->strut.bottom_start_x + 1;
            height = window->strut.reserved.bottom;
        } else {
            width = window->width;
            height = window->height;
        }
    }

    /* consider the window gravity, i.e. where the window wants to be */
    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        adjust_for_window_gravity(monitor, &x, &y, width, height,
            window->size_hints.win_gravity);
    }

    set_window_size(window, x, y, width, height);
}

/* Synchronize the `_NET_WM_ALLOWED_ACTIONS` X property. */
static void synchronize_allowed_actions(Window *window)
{
    const xcb_atom_t atom_lists[WINDOW_MODE_MAX][16] = {
        [WINDOW_MODE_TILING] = {
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CLOSE),
            XCB_NONE,
        },

        [WINDOW_MODE_FLOATING] = {
            ATOM(_NET_WM_ACTION_MOVE),
            ATOM(_NET_WM_ACTION_RESIZE),
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_SHADE),
            ATOM(_NET_WM_ACTION_STICK),
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CLOSE),
            ATOM(_NET_WM_ACTION_ABOVE),
            ATOM(_NET_WM_ACTION_BELOW),
            XCB_NONE,
        },

        [WINDOW_MODE_FULLSCREEN] = {
            ATOM(_NET_WM_ACTION_CLOSE),
            ATOM(_NET_WM_ACTION_ABOVE),
            ATOM(_NET_WM_ACTION_BELOW),
            XCB_NONE,
        },

        [WINDOW_MODE_DOCK] = {
            XCB_NONE,
        },
    };

    const xcb_atom_t *list;
    uint32_t list_length;

    list = atom_lists[window->state.mode];
    for (list_length = 0; list_length < SIZE(atom_lists[0]); list_length++) {
        if (list[list_length] == XCB_NONE) {
            break;
        }
    }

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            window->client.id, ATOM(_NET_WM_ALLOWED_ACTIONS),
            XCB_ATOM_ATOM, 32, list_length, list);
}

/* Add window states to the window properties. */
void add_window_states(Window *window, xcb_atom_t *states,
        uint32_t number_of_states)
{
    uint32_t effective_count = 0;

    /* for each state in `states`, either add it or filter it out */
    for (uint32_t i = 0, j; i < number_of_states; i++) {
        /* filter out the states already in the window properties */
        if (has_state(window, states[i])) {
            continue;
        }

        j = 0;

        /* add the state to the window properties */
        if (window->states != NULL) {
            /* find the number of elements */
            for (; window->states[j] != XCB_NONE; j++) {
                /* nothing */
            }
        }

        RESIZE(window->states, j + 2);
        window->states[j] = states[i];
        window->states[j + 1] = XCB_NONE;

        states[effective_count++] = states[i];
    }

    /* check if anything changed */
    if (effective_count == 0) {
        return;
    }

    /* append the properties to the list in the X server */
    xcb_change_property(connection, XCB_PROP_MODE_APPEND,
            window->client.id, ATOM(_NET_WM_STATE),
            XCB_ATOM_ATOM, 32, effective_count, states);
}

/* Remove window states from the window properties. */
void remove_window_states(Window *window, xcb_atom_t *states,
        uint32_t number_of_states)
{
    uint32_t i;
    uint32_t effective_count = 0;

    /* if no states are there, nothing can be removed */
    if (window->states == NULL) {
        return;
    }

    /* filter out all states in the window properties that are in `states` */
    for (i = 0; window->states[i] != XCB_NONE; i++) {
        uint32_t j;

        /* check if the state exists in `states`... */
        for (j = 0; j < number_of_states; j++) {
            if (states[j] == window->states[i]) {
                break;
            }
        }

        /* ...if not, add it */
        if (j == number_of_states) {
            window->states[effective_count++] =
                window->states[i];
        }
    }

    /* check if anything changed */
    if (effective_count == i) {
        return;
    }

    /* terminate the end with `XCB_NONE` */
    window->states[effective_count] = XCB_NONE;

    /* replace the atom list on the X server */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            window->client.id, ATOM(_NET_WM_STATE),
            XCB_ATOM_ATOM, 32, effective_count,
            window->states);
}

/* Changes the window state to given value and reconfigures the window only
 * if the mode changed.
 */
void set_window_mode(Window *window, window_mode_t mode)
{
    xcb_atom_t states[3];

    if (window->state.mode == mode) {
        return;
    }

    LOG("transition window mode of %W from %m to %m\n", window,
            window->state.mode, mode);

    /* this is true if the window is being initialized */
    if (window->state.mode == WINDOW_MODE_MAX) {
        window->state.previous_mode = mode;
    } else {
        window->state.previous_mode = window->state.mode;
    }
    window->state.mode = mode;

    if (window->state.is_visible) {
        /* pop out from tiling layout */
        if (window->state.previous_mode == WINDOW_MODE_TILING) {
            /* make sure no shortcut is taken in `get_frame_of_window()` */
            window->state.mode = WINDOW_MODE_TILING;
            Frame *const frame = get_frame_of_window(window);
            window->state.mode = mode;
            frame->window = NULL;
            if (configuration.tiling.auto_fill_void) {
                fill_void_with_stash(frame);
            }
        }

        switch (mode) {
        /* replace the focused frame with a frame containing the window */
        case WINDOW_MODE_TILING:
            stash_frame(focus_frame);
            focus_frame->window = window;
            reload_frame(focus_frame);
            break;

        /* set the floating size */
        case WINDOW_MODE_FLOATING:
            configure_floating_size(window);
            break;

        /* set the fullscreen size */
        case WINDOW_MODE_FULLSCREEN:
            configure_fullscreen_size(window);
            break;

        /* set the dock size */
        case WINDOW_MODE_DOCK:
            configure_dock_size(window);
            break;

        /* not a real window mode */
        case WINDOW_MODE_MAX:
            break;
        }
    }

    /* update the window states */
    if (mode == WINDOW_MODE_FULLSCREEN) {
        states[0] = ATOM(_NET_WM_STATE_FULLSCREEN);
        states[1] = ATOM(_NET_WM_STATE_MAXIMIZED_HORZ);
        states[2] = ATOM(_NET_WM_STATE_MAXIMIZED_VERT);
        add_window_states(window, states, SIZE(states));
    } else if (window->state.previous_mode == WINDOW_MODE_FULLSCREEN) {
        states[0] = ATOM(_NET_WM_STATE_FULLSCREEN);
        states[1] = ATOM(_NET_WM_STATE_MAXIMIZED_HORZ);
        states[2] = ATOM(_NET_WM_STATE_MAXIMIZED_VERT);
        remove_window_states(window, states, SIZE(states));
    }

    /* update the window border */
    if (has_window_border(window)) {
        window->border_size = configuration.border.size;
    } else {
        window->border_size = 0;
    }

    update_window_layer(window);

    synchronize_allowed_actions(window);
}

/* Show the window by mapping it to the X server. */
void show_window(Window *window)
{
    if (window->state.is_visible) {
        return;
    }

    switch (window->state.mode) {
    /* the window has to become part of the tiling layout */
    case WINDOW_MODE_TILING: {
        Frame *const frame = get_frame_of_window(window);
        /* we never would want this to happen */
        if (frame != NULL) {
            LOG_ERROR("window %W is already in frame %F\n", window, frame);
            reload_frame(frame);
            break;
        }
        stash_frame(focus_frame);
        focus_frame->window = window;
        reload_frame(focus_frame);
        break;
    }

    /* the window has to show as floating window */
    case WINDOW_MODE_FLOATING:
        configure_floating_size(window);
        break;

    /* the window has to show as fullscreen window */
    case WINDOW_MODE_FULLSCREEN:
        configure_fullscreen_size(window);
        break;

    /* the window has to show as dock window */
    case WINDOW_MODE_DOCK:
        configure_dock_size(window);
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }

    window->state.is_visible = true;
}

/* Hide @window and adjust the tiling and focus. */
void hide_window(Window *window)
{
    Frame *frame, *stash;

    if (!window->state.is_visible) {
        return;
    }

    switch (window->state.mode) {
    /* the window is replaced by another window in the tiling layout */
    case WINDOW_MODE_TILING:
        frame = get_frame_of_window(window);

        stash = stash_frame_later(frame);
        if (configuration.tiling.auto_remove_void) {
            if (frame->parent != NULL) {
                remove_void(frame);
            }
        } else if (configuration.tiling.auto_fill_void) {
            fill_void_with_stash(frame);
            if (window == focus_window) {
                set_focus_window(frame->window);
            }
        }
        link_frame_into_stash(stash);

        /* make sure no broken focus remains */
        if (window == focus_window) {
            set_focus_window(NULL);
        }
        break;

    /* need to just focus a different window */
    case WINDOW_MODE_FLOATING:
    case WINDOW_MODE_FULLSCREEN:
    case WINDOW_MODE_DOCK:
        if (window == focus_window) {
            Window *next;

            /* first get a top window that is visible and not a tiling window */
            for (next = top_window; next != NULL; next = next->below) {
                if (next->state.mode == WINDOW_MODE_TILING) {
                    next = NULL;
                    break;
                }
                if (next != window && next->state.is_visible) {
                    break;
                }
            }

            /* if no such window exists, then we focus the current frame */
            if (next == NULL) {
                set_focus_frame(focus_frame);
            } else {
                set_focus_window_with_frame(next);
            }
        }
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }

    window->state.is_visible = false;
}

/* Hide the window without touching the tiling or focus. */
void hide_window_abruptly(Window *window)
{
    /* do nothing if the window is already hidden */
    if (!window->state.is_visible) {
        return;
    }

    window->state.is_visible = false;

    /* make sure there is no invalid focus window */
    if (window == focus_window) {
        set_focus_window(NULL);
    }
}
