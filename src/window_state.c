#include <inttypes.h>

#include "configuration.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "root_properties.h"
#include "screen.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* The whole purpose of this file is to detect the initial state of a window
 * and to handle a case where a window changes its window state.
 */

#include <signal.h>

/* Predicts what kind of mode the window should be in.
 * TODO: should this be adjustable in the user configuration?
 */
window_mode_t predict_window_mode(Window *window)
{
    /* direct checks */
    if (x_is_state(&window->properties, ATOM(_NET_WM_STATE_FULLSCREEN))) {
        return WINDOW_MODE_FULLSCREEN;
    }
    if (x_is_window_type(&window->properties, ATOM(_NET_WM_WINDOW_TYPE_DOCK))) {
        return WINDOW_MODE_DOCK;
    }

    /* if this window has strut, it must be a dock */
    if (!is_strut_empty(&window->properties.strut)) {
        return WINDOW_MODE_DOCK;
    }

    /* transient windows are popup windows */
    if (window->properties.transient_for != 0) {
        return WINDOW_MODE_POPUP;
    }

    /* popup windows have an equal minimum and maximum size */
    if ((window->properties.size_hints.flags &
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) ==
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) &&
            (window->properties.size_hints.min_width ==
                window->properties.size_hints.max_width ||
            window->properties.size_hints.min_height ==
                window->properties.size_hints.max_height)) {
        return WINDOW_MODE_POPUP;
    }

    /* popup windows have static gravity */
    if ((window->properties.size_hints.flags & (XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) &&
            window->properties.size_hints.win_gravity == XCB_GRAVITY_STATIC) {
        return WINDOW_MODE_POPUP;
    }

    /* popup windows do not have the normal window type */
    if (window->properties.types[0] == ATOM(_NET_WM_WINDOW_TYPE_NORMAL)) {
        return WINDOW_MODE_TILING;
    }

    if (window->properties.types[0] != XCB_NONE) {
        return WINDOW_MODE_POPUP;
    }

    /* popup windows have a maximum size */
    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        return WINDOW_MODE_POPUP;
    }

    /* fall back to tiling window */
    return WINDOW_MODE_TILING;
}

/* Set the window size and position according to the size hints. */
static void configure_popup_size(Window *window)
{
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    monitor = get_monitor_from_rectangle(window->position.x, window->position.y,
            window->size.width, window->size.height);

    /* if the window never had a popup size, use the size hints to get a size
     * that the window prefers
     */
    if (window->popup_size.width == 0) {
        if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
            width = window->properties.size_hints.width;
            height = window->properties.size_hints.height;
        } else {
            width = monitor->size.width * 2 / 3;
            height = monitor->size.height * 2 / 3;
        }

        if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            width = MAX(width, (uint32_t) window->properties.size_hints.min_width);
            height = MAX(height, (uint32_t) window->properties.size_hints.min_height);
        }

        if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
            width = MIN(width, (uint32_t) window->properties.size_hints.max_width);
            height = MIN(height, (uint32_t) window->properties.size_hints.max_height);
        }

        if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION)) {
            x = window->properties.size_hints.x;
            y = window->properties.size_hints.y;
        } else {
            x = monitor->position.x + (monitor->size.width - width) / 2;
            y = monitor->position.y + (monitor->size.height - height) / 2;
        }

        window->popup_position.x = x;
        window->popup_position.y = y;
        window->popup_size.width = width;
        window->popup_size.height = height;
    } else {
        x = window->popup_position.x;
        y = window->popup_position.y;
        width = window->popup_size.width;
        height = window->popup_size.height;
    }

    /* consider the window gravity, i.e. where the window wants to be */
    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        switch (window->properties.size_hints.win_gravity) {
        case XCB_GRAVITY_NORTH_WEST:
            x = monitor->position.x;
            y = monitor->position.y;
            break;

        case XCB_GRAVITY_NORTH:
            y = monitor->position.y;
            break;

        case XCB_GRAVITY_NORTH_EAST:
            x = monitor->position.x + monitor->size.width - width;
            y = monitor->position.y;
            break;

        case XCB_GRAVITY_WEST:
            x = monitor->position.x;
            break;

        case XCB_GRAVITY_CENTER:
            x = monitor->position.x + (monitor->size.width - width) / 2;
            y = monitor->position.y + (monitor->size.height - height) / 2;
            break;

        case XCB_GRAVITY_EAST:
            x = monitor->position.x + monitor->size.width - width;
            break;

        case XCB_GRAVITY_SOUTH_WEST:
            x = monitor->position.x;
            y = monitor->position.y + monitor->size.height - height;
            break;

        case XCB_GRAVITY_SOUTH:
            y = monitor->position.y + monitor->size.height - height;
            break;

        case XCB_GRAVITY_SOUTH_EAST:
            x = monitor->position.x + monitor->size.width - width;
            y = monitor->position.y + monitor->size.width - height;
            break;
        }
    }

    set_window_size(window, x, y, width, height);

    /* make sure the popup window has a border */
    general_values[0] = configuration.border.size;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
}

/* Sets the position and size of the window to fullscreen. */
static void configure_fullscreen_size(Window *window)
{
    Monitor *monitor;

    if (window->properties.fullscreen_monitor.top !=
            window->properties.fullscreen_monitor.bottom) {
        set_window_size(window, window->properties.fullscreen_monitor.left,
                window->properties.fullscreen_monitor.top,
                window->properties.fullscreen_monitor.right -
                    window->properties.fullscreen_monitor.left,
                window->properties.fullscreen_monitor.bottom -
                    window->properties.fullscreen_monitor.left);
    } else {
        monitor = get_monitor_from_rectangle(window->position.x,
                window->position.y, window->size.width, window->size.height);
        set_window_size(window, monitor->position.x, monitor->position.y,
                monitor->size.width, monitor->size.height);
    }

    /* remove border: fullscreen windows have no border */
    general_values[0] = 0;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
}



/* Sets the position and size of the window to a dock window. */
static void configure_dock_size(Window *window)
{
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        width = window->properties.size_hints.width;
        height = window->properties.size_hints.height;
    } else {
        width = 0;
        height = 0;
    }

    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION)) {
        x = window->properties.size_hints.x;
        y = window->properties.size_hints.y;
    } else {
        x = window->position.x;
        y = window->position.y;
    }

    monitor = get_monitor_from_rectangle(x, y, 1, 1);

    /* if the window does not specify a size itself, then do it based on the
     * strut the window defines, reasoning is that when the window wants to
     * occupy screen space, then it should be within that occupied space
     */
    if (width == 0 || height == 0) {
        if (window->properties.strut.reserved.left != 0) {
            x = monitor->position.x;
            y = window->properties.strut.left_start_y;
            width = window->properties.strut.reserved.left;
            height = window->properties.strut.left_end_y -
                window->properties.strut.left_start_y + 1;
        } else if (window->properties.strut.reserved.top != 0) {
            x = window->properties.strut.top_start_x;
            y = monitor->position.y;
            width = window->properties.strut.top_end_x -
                window->properties.strut.top_start_x + 1;
            height = window->properties.strut.reserved.top;
        } else if (window->properties.strut.reserved.right != 0) {
            x = monitor->position.x + monitor->size.width -
                window->properties.strut.reserved.right;
            y = window->properties.strut.right_start_y;
            width = window->properties.strut.reserved.right;
            height = window->properties.strut.right_end_y -
                window->properties.strut.right_start_y + 1;
        } else if (window->properties.strut.reserved.bottom != 0) {
            x = window->properties.strut.bottom_start_x;
            y = monitor->position.y + monitor->size.height -
                window->properties.strut.reserved.bottom;
            width = window->properties.strut.bottom_end_x -
                window->properties.strut.bottom_start_x + 1;
            height = window->properties.strut.reserved.bottom;
        } else {
            /* TODO: what to do here? */
            width = 64;
            height = 32;
        }
    }

    /* consider the window gravity, i.e. where the window wants to be */
    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        switch (window->properties.size_hints.win_gravity) {
        case XCB_GRAVITY_NORTH_WEST:
            x = monitor->position.x;
            break;

        case XCB_GRAVITY_NORTH:
            y = monitor->position.y;
            break;

        case XCB_GRAVITY_NORTH_EAST:
            x = monitor->position.x + monitor->size.width - width;
            y = monitor->position.y;
            break;

        case XCB_GRAVITY_WEST:
            x = monitor->position.x;
            break;

        case XCB_GRAVITY_CENTER:
            x = monitor->position.x + (monitor->size.width - width) / 2;
            y = monitor->position.y + (monitor->size.height - height) / 2;
            break;

        case XCB_GRAVITY_EAST:
            x = monitor->position.x + monitor->size.width - width;
            break;

        case XCB_GRAVITY_SOUTH_WEST:
            x = monitor->position.x;
            y = monitor->position.y + monitor->size.height - height;
            break;

        case XCB_GRAVITY_SOUTH:
            y = monitor->position.y + monitor->size.height - height;
            break;

        case XCB_GRAVITY_SOUTH_EAST:
            x = monitor->position.x + monitor->size.width - width;
            y = monitor->position.y + monitor->size.width - height;
            break;
        }
    }

    set_window_size(window, x, y, width, height);

    /* dock windows have no border */
    general_values[0] = 0;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
}

/* Synchronize the _NET_WM_ALLOWED actions X property. */
void synchronize_allowed_actions(Window *window)
{
    const xcb_atom_t atom_lists[WINDOW_MODE_MAX][16] = {
        [WINDOW_MODE_TILING] = {
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CHANGE_DESKTOP),
            ATOM(_NET_WM_ACTION_CLOSE),
            XCB_NONE,
        },

        [WINDOW_MODE_POPUP] = {
            ATOM(_NET_WM_ACTION_MOVE),
            ATOM(_NET_WM_ACTION_RESIZE),
            ATOM(_NET_WM_ACTION_MINIMIZE),
            ATOM(_NET_WM_ACTION_SHADE),
            ATOM(_NET_WM_ACTION_STICK),
            ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ),
            ATOM(_NET_WM_ACTION_MAXIMIZE_VERT),
            ATOM(_NET_WM_ACTION_FULLSCREEN),
            ATOM(_NET_WM_ACTION_CHANGE_DESKTOP),
            ATOM(_NET_WM_ACTION_CLOSE),
            ATOM(_NET_WM_ACTION_ABOVE),
            ATOM(_NET_WM_ACTION_BELOW),
            XCB_NONE,
        },

        [WINDOW_MODE_FULLSCREEN] = {
            ATOM(_NET_WM_ACTION_CHANGE_DESKTOP),
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
            window->properties.window, ATOM(_NET_WM_ALLOWED_ACTIONS),
            XCB_ATOM_ATOM, 32, list_length * sizeof(*list), list);
}

/* Changes the window state to given value and reconfigures the window only
 * if the mode changed.
 */
void set_window_mode(Window *window, window_mode_t mode, bool force_mode)
{
    if (window->state.mode == mode ||
            (window->state.is_mode_forced && !force_mode)) {
        return;
    }

    LOG("transition window mode of %" PRIu32 " from %u to %u (%s)\n",
            window->number, window->state.mode, mode,
            force_mode ? "forced" : "not forced");

    window->state.is_mode_forced = force_mode;

    if (window->state.is_visible) {
        /* pop out from tiling layout */
        if (window->state.mode == WINDOW_MODE_TILING) {
            Frame *const frame = get_frame_of_window(window);
            if (configuration.tiling.auto_fill_void) {
                Window *const next = get_next_hidden_window(window);
                if (next != NULL) {
                    frame->window = next;
                    reload_frame(frame);
                    show_window_quickly(next);
                } else {
                    frame->window = NULL;
                }
            } else {
                frame->window = NULL;
            }
        }

        switch (mode) {
        case WINDOW_MODE_TILING: {
            Window *const old_window = focus_frame->window;

            focus_frame->window = window;
            reload_frame(focus_frame);

            set_focus_window(window);

            if (old_window != NULL) {
                hide_window_quickly(old_window);
            }

            /* tiling windows have a border */
            general_values[0] = configuration.border.size;
            xcb_configure_window(connection, window->properties.window,
                    XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
        } break;

        case WINDOW_MODE_POPUP:
            configure_popup_size(window);
            break;

        case WINDOW_MODE_FULLSCREEN:
            configure_fullscreen_size(window);
            break;

        case WINDOW_MODE_DOCK:
            configure_dock_size(window);
            break;

        /* not a real window mode */
        case WINDOW_MODE_MAX:
            break;
        }

        set_window_above(window);
    }

    window->state.previous_mode = window->state.mode;
    window->state.mode = mode;

    synchronize_allowed_actions(window);
}

/* Show given window but do not assign an id and no size configuring. */
void show_window_quickly(Window *window)
{
    if (window->state.is_visible) {
        return;
    }

    LOG("quickly showing window with id: %" PRIu32 "\n", window->number);

    window->state.is_visible = true;

    xcb_map_window(connection, window->properties.window);

    general_values[0] = configuration.border.color;
    xcb_change_window_attributes(connection, window->properties.window,
            XCB_CW_BORDER_PIXEL, general_values);

    /* check if strut have disappeared */
    if (!is_strut_empty(&window->properties.strut)) {
        reconfigure_monitor_frame_sizes();
        synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
    }
}

/* Show the window by mapping it to the X server. */
void show_window(Window *window)
{
    Window *last, *previous;

    if (window->state.is_visible) {
        return;
    }

    /* assign the first id to the window if it is first mapped */
    if (!window->state.was_ever_mapped) {
        for (last = first_window; last->next != NULL; last = last->next) {
            if (last->number + 1 < last->next->number) {
                break;
            }
        }
        window->number = last->number + 1;

        LOG("assigned id %" PRIu32 " to window wrapping %" PRIu32 "\n",
                window->number, window->properties.window);

        if (last != window) {
            if (window != first_window) {
                previous = get_previous_window(window);
                previous->next = window->next;
            } else {
                first_window = window->next;
            }
            window->next = last->next;
            last->next = window;
        }

        window->state.was_ever_mapped = true;
    }

    LOG("showing window with id: %" PRIu32 "\n", window->number);

    window->state.is_visible = true;

    previous = NULL;
    switch (window->state.mode) {
    /* the window has to become part of the tiling layout */
    case WINDOW_MODE_TILING:
        previous = focus_frame->window;

        focus_frame->window = window;
        reload_frame(focus_frame);

        general_values[0] = configuration.border.size;
        xcb_configure_window(connection, window->properties.window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
        break;

    /* the window has to show as popup window */
    case WINDOW_MODE_POPUP:
        configure_popup_size(window);
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

    xcb_map_window(connection, window->properties.window);

    if (previous != NULL) {
        hide_window_quickly(previous);
    }

    /* check if strut have appeared */
    if (!is_strut_empty(&window->properties.strut)) {
        reconfigure_monitor_frame_sizes();
        synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
    }
}

/* Hide a window without focusing another window. */
void hide_window_quickly(Window *window)
{
    Frame *frame;

    if (!window->state.is_visible) {
        return;
    }

    LOG("quickly hiding window with id: %" PRIu32 "\n", window->number);

    window->state.is_visible = false;

    frame = get_frame_of_window(window);
    if (frame != NULL) {
        frame->window = NULL;
    }

    if (focus_window == window) {
        set_focus_window_primitively(window->previous_focus);
    }

    unlink_window_from_focus_list(window);

    xcb_unmap_window(connection, window->properties.window);

    /* check if strut has disappeared */
    if (!is_strut_empty(&window->properties.strut)) {
        reconfigure_monitor_frame_sizes();
        synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
    }
}

/* Focus the window that was in focus before the currently focused one. */
static void focus_previous_window(void)
{
    if (focus_window->previous_focus != focus_window) {
        set_focus_window(focus_window->previous_focus);
    } else {
        set_focus_window_primitively(NULL);
        synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);
    }
}

/* Hide the window by unmapping it from the X server. */
void hide_window(Window *window)
{
    Window *next;
    Frame *frame;

    if (!window->state.is_visible) {
        return;
    }

    LOG("hiding window with id: %" PRIu32 "\n", window->number);

    window->state.is_visible = false;

    switch (window->state.mode) {
    /* the window is replaced by another window in the tiling layout */
    case WINDOW_MODE_TILING:
        frame = get_frame_of_window(window);
        if (configuration.tiling.auto_remove_void) {
            if (frame->parent != NULL) {
                remove_frame(frame);
            }
            break;
        }

        frame->window = NULL;
        if (configuration.tiling.auto_fill_void) {
            next = get_next_hidden_window(window);
            if (next != NULL) {
                frame->window = next;
                reload_frame(frame);
                show_window_quickly(next);
                if (window == focus_window) {
                    set_focus_window(next);
                }
            }
        }
    /* fall through */
    /* need to just focus a different window */
    case WINDOW_MODE_POPUP:
    case WINDOW_MODE_FULLSCREEN:
    case WINDOW_MODE_DOCK:
        if (window == focus_window) {
            focus_previous_window();
        }
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }

    unlink_window_from_focus_list(window);

    xcb_unmap_window(connection, window->properties.window);

    /* check if strut has appeared */
    if (!is_strut_empty(&window->properties.strut)) {
        reconfigure_monitor_frame_sizes();
        synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
    }
}
