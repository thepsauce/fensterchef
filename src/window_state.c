#include <inttypes.h>

#include "configuration.h"
#include "frame.h"
#include "frame_splitting.h"
#include "frame_stashing.h"
#include "log.h"
#include "monitor.h"
#include "utility/utility.h"
#include "window.h"
#include "window_properties.h"
#include "window_stacking.h"
#include "x11_synchronize.h"

/* The whole purpose of this file is to handle window state changes
 * This includes visibility and window mode.
 */

/* Check if @window should have no border. */
bool is_window_borderless(FcWindow *window)
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
int get_window_gravity(FcWindow *window)
{
    if (window->strut.left > 0) {
        return WestGravity;
    }
    if (window->strut.top > 0) {
        return NorthGravity;
    }
    if (window->strut.right > 0) {
        return EastGravity;
    }
    if (window->strut.bottom > 0) {
        return SouthGravity;
    }

    if ((window->size_hints.flags & PWinGravity)) {
        return window->size_hints.win_gravity;
    }
    return StaticGravity;
}

/* Return a comparison value for two windows. */
static int sort_by_x(const void *a, const void *b)
{
    const FcWindow *const window_a = *(FcWindow**) a;
    const FcWindow *const window_b = *(FcWindow**) b;
    return window_a->x - window_b->x;
}

/* Put windows along a diagonal line, spacing them out a little. */
static inline void move_to_next_available(Monitor *monitor, FcWindow *window,
        int *x, int *y)
{
    /* step 1: get all windows on the diagonal line */
    FcWindow *in_line_windows[Window_count];
    unsigned count = 0;
    int best_x = 0;

    *x = monitor->x + monitor->width / 10;
    *y = monitor->y + monitor->height / 10;

    for (FcWindow *other = Window_first; other != NULL; other = other->next) {
        if (other == window || !other->state.is_visible) {
            continue;
        }
        const Point difference = {
            other->x - *x,
            other->y - *y,
        };
        if (difference.x >= 0 && difference.x == difference.y &&
                difference.x % 20 == 0) {
            if (best_x == difference.x) {
                best_x += 20;
            }
            in_line_windows[count] = other;
            count++;
        }
    }

    /* intermediary step: if the windows were already sorted advantageously,
     * immediately find a gap
     */
    for (FcWindow *other = Window_first; other != NULL; other = other->next) {
        const Point difference = {
            other->x - *x,
            other->y - *y,
        };
        if (difference.x == best_x) {
            best_x = -1;
            break;
        }
    }

    if (best_x >= 0) {
        *x += best_x;
        *y += best_x;
        return;
    }

    /* step 2: sort them according to their x coordinate */
    qsort(in_line_windows, count, sizeof(*in_line_windows), sort_by_x);

    /* step 3: find a gap */
    for (unsigned i = 0; i < count; i++) {
        FcWindow *const other = in_line_windows[i];
        if (other->x != *x || other->y != *y) {
            break;
        }
        *x += 20;
        *y += 20;
    }
}

/* Set the window size and position according to the size hints. */
static void configure_floating_size(FcWindow *window)
{
    int x, y;
    unsigned width, height;

    /* if the window never had a floating size, figure it out based off the
     * hints
     */
    if (window->floating.width == 0) {
        Monitor *monitor;

        /* put the window on the monitor that is either on the same monitor as
         * the focused window or the focused frame
         */
        if (Window_focus != NULL) {
            monitor = get_monitor_containing_window(Window_focus);
        } else {
            monitor = get_monitor_containing_frame(Frame_focus);
        }

        if (window->floating.width > 0) {
            width = window->floating.width;
            height = window->floating.height;
        } else if ((window->size_hints.flags & PSize)) {
            width = window->size_hints.width;
            height = window->size_hints.height;
        } else {
            width = monitor->width * 2 / 3;
            height = monitor->height * 2 / 3;
        }

        if ((window->size_hints.flags & PMinSize)) {
            width = MAX(width, (unsigned) window->size_hints.min_width);
            height = MAX(height, (unsigned) window->size_hints.min_height);
        }

        if ((window->size_hints.flags & PMaxSize)) {
            width = MIN(width, (unsigned) window->size_hints.max_width);
            height = MIN(height, (unsigned) window->size_hints.max_height);
        }

        /* non resizable windows are centered */
        if ((window->size_hints.flags & (PMinSize | PMaxSize)) ==
                    (PMinSize | PMaxSize) &&
                (window->size_hints.min_width == window->size_hints.max_width ||
                    window->size_hints.min_height ==
                        window->size_hints.max_height)) {
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
static void configure_fullscreen_size(FcWindow *window)
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
        monitor = get_monitor_containing_window(window);
        set_window_size(window, monitor->x, monitor->y,
                monitor->width, monitor->height);
    }
}

/* Set the position and size of the window to a dock window. */
void configure_dock_size(FcWindow *window)
{
    Monitor *monitor;
    int x, y;
    unsigned width, height;

    monitor = get_monitor_containing_window(window);

    if (!is_strut_empty(&window->strut)) {
        x = monitor->x;
        y = monitor->y;
        width = monitor->width;
        height = monitor->height;

        /* do the sizing/position based on the strut the window defines,
         * reasoning is that when the window wants to occupy screen space, then
         * it should be within that occupied space
         */
        if (window->strut.left != 0) {
            width = window->strut.left;
            /* check if the extended strut is set or if it is malformed */
            if (window->strut.left_start_y < window->strut.left_end_y) {
                y = window->strut.left_start_y;
                height = window->strut.left_end_y -
                    window->strut.left_start_y + 1;
            }
        } else if (window->strut.top != 0) {
            height = window->strut.top;
            if (window->strut.top_start_x < window->strut.top_end_x) {
                x = window->strut.top_start_x;
                width = window->strut.top_end_x - window->strut.top_start_x + 1;
            }
        } else if (window->strut.right != 0) {
            x = monitor->x + monitor->width - window->strut.right;
            width = window->strut.right;
            if (window->strut.right_start_y < window->strut.right_end_y) {
                y = window->strut.right_start_y;
                height = window->strut.right_end_y -
                    window->strut.right_start_y + 1;
            }
        } else if (window->strut.bottom != 0) {
            y = monitor->y + monitor->height - window->strut.bottom;
            height = window->strut.bottom;
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

        const int gravity = get_window_gravity(window);
        adjust_for_window_gravity(monitor, &x, &y, width, height, gravity);
    }

    set_window_size(window, x, y, width, height);
}

/* Synchronize the `_NET_WM_ALLOWED_ACTIONS` X property. */
static void synchronize_allowed_actions(FcWindow *window)
{
    const Atom atom_lists[WINDOW_MODE_MAX][16] = {
        [WINDOW_MODE_TILING] = {
            ATOM(_NET_WM_ACTION_MOVE),
            ATOM(_NET_WM_ACTION_RESIZE),
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_CLOSE),
            None,
        },

        [WINDOW_MODE_FLOATING] = {
            ATOM(_NET_WM_ACTION_MOVE),
            ATOM(_NET_WM_ACTION_RESIZE),
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_CLOSE),
            ATOM(_NET_WM_ACTION_ABOVE),
            None,
        },

        [WINDOW_MODE_FULLSCREEN] = {
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_CLOSE),
            ATOM(_NET_WM_ACTION_ABOVE),
            None,
        },

        [WINDOW_MODE_DOCK] = {
            None,
        },

        [WINDOW_MODE_DESKTOP] = {
            None,
        },
    };

    const Atom *list;
    unsigned list_length;

    list = atom_lists[window->state.mode];
    for (list_length = 0; list_length < SIZE(atom_lists[0]); list_length++) {
        if (list[list_length] == None) {
            break;
        }
    }

    XChangeProperty(display, window->client.id, ATOM(_NET_WM_ALLOWED_ACTIONS),
            XA_ATOM, 32, PropModeReplace, (unsigned char*) list, list_length);
}

/* Add window states to the window properties. */
void add_window_states(FcWindow *window, Atom *states,
        unsigned number_of_states)
{
    unsigned effective_count = 0;

    /* for each state in `states`, either add it or filter it out */
    for (unsigned i = 0, j; i < number_of_states; i++) {
        /* filter out the states already in the window properties */
        if (has_state(window, states[i])) {
            continue;
        }

        j = 0;

        /* add the state to the window properties */
        if (window->states != NULL) {
            /* find the number of elements */
            for (; window->states[j] != None; j++) {
                /* nothing */
            }
        }

        REALLOCATE(window->states, j + 2);
        window->states[j] = states[i];
        window->states[j + 1] = None;

        states[effective_count++] = states[i];
    }

    /* check if anything changed */
    if (effective_count == 0) {
        return;
    }

    /* append the properties to the list in the X server */
    XChangeProperty(display, window->client.id, ATOM(_NET_WM_STATE), XA_ATOM,
            32, PropModeAppend, (unsigned char*) states, effective_count);
}

/* Remove window states from the window properties. */
void remove_window_states(FcWindow *window, Atom *states,
        unsigned number_of_states)
{
    unsigned i;
    unsigned effective_count = 0;

    /* if no states are there, nothing can be removed */
    if (window->states == NULL) {
        return;
    }

    /* filter out all states in the window properties that are in `states` */
    for (i = 0; window->states[i] != None; i++) {
        unsigned j;

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

    /* terminate the end with `None` */
    window->states[effective_count] = None;

    /* replace the atom list on the X server */
    XChangeProperty(display, window->client.id, ATOM(_NET_WM_STATE), XA_ATOM,
            32, PropModeReplace,
            (unsigned char*) window->states, effective_count);
}

/* When a window changes mode or is shown, this is called.
 *
 * This adjusts the window size or puts the window into the tiling layout.
 */
static void update_shown_window(FcWindow *window)
{
    switch (window->state.mode) {
    /* the window has to become part of the tiling layout */
    case WINDOW_MODE_TILING: {
        Frame *frame;

        frame = get_window_frame(window);
        /* we never would want this to happen */
        if (frame != NULL) {
            LOG_ERROR("window %W is already in frame %F\n",
                    window, frame);
            reload_frame(frame);
            break;
        }

        frame = get_frame_by_number(window->number);
        if (frame != NULL) {
            LOG("found frame %F matching the window id\n",
                    frame);
            (void) stash_frame(frame);
            frame->window = window;
            reload_frame(frame);
            break;
        }

        if (configuration.auto_split && Frame_focus->window != NULL) {
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
void set_window_mode(FcWindow *window, window_mode_t mode)
{
    Atom states[3];

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
            /* make sure no shortcut is taken in `get_window_frame()` */
            window->state.mode = WINDOW_MODE_TILING;
            Frame *const frame = get_window_frame(window);
            window->state.mode = mode;

            frame->window = NULL;
            if (configuration.auto_remove ||
                    configuration.auto_remove_void) {
                /* do not remove a root */
                if (frame->parent != NULL) {
                    remove_frame(frame);
                    destroy_frame(frame);
                }
            } else if (configuration.auto_fill_void) {
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

    update_window_layer(window);

    synchronize_allowed_actions(window);
}

/* Show the window by mapping it to the X server. */
void show_window(FcWindow *window)
{
    if (window->state.is_visible) {
        return;
    }

    update_shown_window(window);

    window->state.is_visible = true;
}

/* Hide @window and adjust the tiling and focus. */
void hide_window(FcWindow *window)
{
    Frame *frame, *stash;

    if (!window->state.is_visible) {
        return;
    }

    switch (window->state.mode) {
    /* the window is replaced by another window in the tiling layout */
    case WINDOW_MODE_TILING: {
        Frame *pop;

        frame = get_window_frame(window);

        pop = pop_stashed_frame();

        stash = stash_frame_later(frame);
        if (configuration.auto_remove) {
            /* if the frame is not a root frame, remove it, otherwise
             * auto-fill-void is checked
             */
            if (frame->parent != NULL) {
                remove_frame(frame);
                destroy_frame(frame);
            } else if (configuration.auto_fill_void) {
                if (pop != NULL) {
                    replace_frame(frame, pop);
                }
            }
        } else if (configuration.auto_remove_void) {
            /* this option takes precedence */
            if (configuration.auto_fill_void) {
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
        } else if (configuration.auto_fill_void) {
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
        set_focus_window(Frame_focus->window);
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }

    window->state.is_visible = false;
}

/* Hide the window without touching the tiling or focus. */
void hide_window_abruptly(FcWindow *window)
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
