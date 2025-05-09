#include <inttypes.h>

#include "configuration.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"
#include "window_properties.h"

/* The whole purpose of this file is to handle window state changes
 * This includes visibility and window mode.
 */

/* Check if @window should have no border. */
bool is_window_borderless(Window *window)
{
    /* tiling windows always have a border */
    if (window->state.mode == WINDOW_MODE_TILING) {
        return false;
    }
    /* fullscreen, dock and desktop windows have no border */
    if (window->state.mode != WINDOW_MODE_FLOATING) {
        return true;
    }
    return window->is_borderless;
}

/* Get the side of a monitor @window would like to attach to. */
xcb_gravity_t get_window_gravity(Window *window)
{
    if (window->strut.reserved.left > 0) {
        return XCB_GRAVITY_WEST;
    }
    if (window->strut.reserved.top > 0) {
        return XCB_GRAVITY_NORTH;
    }
    if (window->strut.reserved.right > 0) {
        return XCB_GRAVITY_EAST;
    }
    if (window->strut.reserved.bottom > 0) {
        return XCB_GRAVITY_SOUTH;
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        return window->size_hints.win_gravity;
    }
    return XCB_GRAVITY_STATIC;
}

/* Return a comparison value for two windows. */
static int sort_by_x(const void *a, const void *b)
{
    const Window *const window_a = *(Window**) a;
    const Window *const window_b = *(Window**) b;
    return window_a->x - window_b->x;
}

/* Put windows along a diagonal line, spacing them out a little. */
static inline void move_to_next_available(Monitor *monitor, Window *window,
        int *x, int *y)
{
    /* step 1: get all windows on the diagonal line */
    Window *in_line_windows[Window_count];
    uint32_t count = 0;

    *x = monitor->x + monitor->width / 10;
    *y = monitor->y + monitor->height / 10;

    for (Window *other = Window_first; other != NULL; other = other->next) {
        if (other == window || !other->state.is_visible) {
            continue;
        }
        if ((other->x - *x) % 20 != 0 || (other->y - *y) % 20 != 0) {
            continue;
        }
        in_line_windows[count] = other;
        count++;
    }

    /* step 2: sort them according to their x coordinate */
    qsort(in_line_windows, count, sizeof(*in_line_windows), sort_by_x);

    /* step 3: find a gap */
    for (uint32_t i = 0; i < count; i++) {
        if (in_line_windows[i]->x != *x || in_line_windows[i]->y != *y) {
            break;
        }
        *x += 20;
        *y += 20;
    }
}

/* Set the window size and position according to the size hints. */
static void configure_floating_size(Window *window)
{
    Monitor *monitor, *original_monitor = NULL;
    int32_t x, y;
    uint32_t width, height;

    /* put the window on the monitor that is either on the same monitor as the
     * focused window or the focused frame
     */
    if (Window_focus != NULL) {
        monitor = get_monitor_from_rectangle_or_primary(Window_focus->x,
                Window_focus->y, Window_focus->width, Window_focus->height);
    } else {
        monitor = get_monitor_containing_frame(Frame_focus);
    }

    if (window->floating.width > 0) {
        /* the monitor the window was on before */
        original_monitor = get_monitor_from_rectangle(window->floating.x,
                window->floating.y, window->floating.width,
                window->floating.height);
    }

    /* if the window never had a floating size or it would be shown on a monitor
     * different from the focused monitor, use the size hints to get a size that
     * the window prefers
     */
    if (monitor != original_monitor) {
        if (window->floating.width > 0) {
            width = window->floating.width;
            height = window->floating.height;
        } else if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
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

        /* non resizable windows are centered */
        if ((window->size_hints.flags &
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
                 XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) ==
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
                 XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) &&
                (window->size_hints.min_width == window->size_hints.max_width ||
                 window->size_hints.min_height == window->size_hints.max_height)) {
            x = monitor->x + (monitor->width - width) / 2;
            y = monitor->y + (monitor->height - height) / 2;
        } else {
            move_to_next_available(monitor, window, &x, &y);
        }
    } else {
        x = window->floating.x;
        y = window->floating.y;
        width = window->floating.width;
        height = window->floating.height;
    }

    set_window_size(window, x, y, width, height);
}

/* Set the position and size of the window to fullscreen. */
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

/* Set the position and size of the window to a dock window. */
void configure_dock_size(Window *window)
{
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    monitor = get_monitor_from_rectangle_or_primary(window->x, window->y, 1, 1);

    if (!is_strut_empty(&window->strut)) {
        x = monitor->x;
        y = monitor->y;
        width = monitor->width;
        height = monitor->height;

        /* do the sizing/position based on the strut the window defines,
         * reasoning is that when the window wants to occupy screen space, then
         * it should be within that occupied space
         */
        if (window->strut.reserved.left != 0) {
            width = window->strut.reserved.left;
            /* check if the extended strut is set or if it is malformed */
            if (window->strut.left_start_y < window->strut.left_end_y) {
                y = window->strut.left_start_y;
                height = window->strut.left_end_y -
                    window->strut.left_start_y + 1;
            }
        } else if (window->strut.reserved.top != 0) {
            height = window->strut.reserved.top;
            if (window->strut.top_start_x < window->strut.top_end_x) {
                x = window->strut.top_start_x;
                width = window->strut.top_end_x - window->strut.top_start_x + 1;
            }
        } else if (window->strut.reserved.right != 0) {
            x = monitor->x + monitor->width - window->strut.reserved.right;
            width = window->strut.reserved.right;
            if (window->strut.right_start_y < window->strut.right_end_y) {
                y = window->strut.right_start_y;
                height = window->strut.right_end_y -
                    window->strut.right_start_y + 1;
            }
        } else if (window->strut.reserved.bottom != 0) {
            y = monitor->y + monitor->height - window->strut.reserved.bottom;
            height = window->strut.reserved.bottom;
            if (window->strut.bottom_start_x < window->strut.bottom_end_x) {
                x = window->strut.bottom_start_x;
                width = window->strut.bottom_end_x -
                    window->strut.bottom_start_x + 1;
            }
        }
    } else {
        x = window->x;
        y = window->y;
        width = window->width;
        height = window->height;

        const xcb_gravity_t gravity = get_window_gravity(window);
        adjust_for_window_gravity(monitor, &x, &y, width, height, gravity);
    }

    set_window_size(window, x, y, width, height);
}

/* Synchronize the `_NET_WM_ALLOWED_ACTIONS` X property. */
static void synchronize_allowed_actions(Window *window)
{
    const xcb_atom_t atom_lists[WINDOW_MODE_MAX][16] = {
        [WINDOW_MODE_TILING] = {
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CLOSE),
            XCB_NONE,
        },

        [WINDOW_MODE_FLOATING] = {
            ATOM(_NET_WM_ACTION_MOVE),
            ATOM(_NET_WM_ACTION_RESIZE),
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CLOSE),
            XCB_NONE,
        },

        [WINDOW_MODE_FULLSCREEN] = {
            ATOM(_NET_WM_ACTION_CLOSE),
            XCB_NONE,
        },

        [WINDOW_MODE_DOCK] = {
            XCB_NONE,
        },

        [WINDOW_MODE_DESKTOP] = {
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
            window->states[effective_count++] = window->states[i];
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

/* When a window changes mode or is shown, this is called.
 *
 * This adjusts the window size or puts the window into the tiling layout.
 */
static void update_shown_window(Window *window)
{
    switch (window->state.mode) {
    /* the window has to become part of the tiling layout */
    case WINDOW_MODE_TILING: {
        Frame *frame;

        frame = get_frame_of_window(window);
        /* we never would want this to happen */
        if (frame != NULL) {
            LOG_ERROR("window %W is already in frame %F\n", window, frame);
            reload_frame(frame);
            break;
        }

        frame = get_frame_by_number(window->number);
        if (frame != NULL) {
            LOG("found frame %F matching the window id\n", frame);
            (void) stash_frame(frame);
            frame->window = window;
            reload_frame(frame);
            break;
        }

        if (configuration.tiling.auto_split && Frame_focus->window != NULL) {
            Frame *const wrap = create_frame();
            wrap->window = window;
            split_frame(Frame_focus, wrap, false, Frame_focus->split_direction);
            Frame_focus = wrap;
        } else {
            stash_frame(Frame_focus);
            Frame_focus->window = window;
            reload_frame(Frame_focus);
        }
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

    /* do nothing, the desktop window should know better */
    case WINDOW_MODE_DESKTOP:
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }
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
            if (configuration.tiling.auto_remove ||
                    configuration.tiling.auto_remove_void) {
                /* do not remove a root */
                if (frame->parent != NULL) {
                    remove_frame(frame);
                    destroy_frame(frame);
                }
            } else if (configuration.tiling.auto_fill_void) {
                fill_void_with_stash(frame);
            }
        }

        update_shown_window(window);
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
    if (is_window_borderless(window)) {
        window->border_size = 0;
    } else {
        window->border_size = configuration.border.size;
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

    update_shown_window(window);

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
    case WINDOW_MODE_TILING: {
        Frame *pop;

        frame = get_frame_of_window(window);

        pop = pop_stashed_frame();

        stash = stash_frame_later(frame);
        if (configuration.tiling.auto_remove) {
            /* if the frame is not a root frame, remove it, otherwise
             * auto-fill-void is checked
             */
            if (frame->parent != NULL) {
                remove_frame(frame);
                destroy_frame(frame);
            } else if (configuration.tiling.auto_fill_void) {
                if (pop != NULL) {
                    replace_frame(frame, pop);
                }
            }
        } else if (configuration.tiling.auto_remove_void) {
            /* this option takes precedence */
            if (configuration.tiling.auto_fill_void) {
                if (pop != NULL) {
                    replace_frame(frame, pop);
                }
                /* if the frame is no root and kept on being a void, remove it
                 */
                if (frame->parent != NULL && is_frame_void(frame)) {
                    remove_frame(frame);
                    destroy_frame(frame);
                }
            } else if (frame->parent != NULL) {
                remove_frame(frame);
                destroy_frame(frame);
            }
        } else if (configuration.tiling.auto_fill_void) {
            if (pop != NULL) {
                replace_frame(frame, pop);
            }
        }

        /* put `pop` back onto the stack, if it was used it will be empty and
         * therefore not be put onto the stack again but it will in any case be
         * destroyed
         */
        if (pop != NULL) {
            stash_frame(pop);
            destroy_frame(pop);
        }

        link_frame_into_stash(stash);

        /* if nothing is focused, focus the focused frame window */
        if (Window_focus == NULL) {
            set_focus_window(Frame_focus->window);
        }
        break;
    }

    /* need to just focus a different window */
    case WINDOW_MODE_FLOATING:
    case WINDOW_MODE_FULLSCREEN:
    case WINDOW_MODE_DOCK:
    case WINDOW_MODE_DESKTOP:
        if (window == Window_focus) {
            set_focus_frame(Frame_focus);
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
    if (window == Window_focus) {
        set_focus_window(NULL);
    }
}
