#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "screen.h"
#include "util.h"
#include "window.h"

/* The whole purpose of this file is to detect the initial state of a window
 * and to handle a case where a window changes its window state.
 *
 * Since a change from one state to another depends on the previous state,
 * the transition system was created with function pointers.
 */

static void transition_tiling_popup(Window *window);
static void transition_tiling_fullscreen(Window *window);

static void transition_popup_tiling(Window *window);
static void transition_popup_fullscreen(Window *window);

static void transition_fullscreen_popup(Window *window);

static void (*transitions[3][3])(Window *window) = {
    [WINDOW_MODE_TILING][WINDOW_MODE_POPUP] = transition_tiling_popup,
    [WINDOW_MODE_TILING][WINDOW_MODE_FULLSCREEN] = transition_tiling_fullscreen,

    [WINDOW_MODE_POPUP][WINDOW_MODE_TILING] = transition_popup_tiling,
    [WINDOW_MODE_POPUP][WINDOW_MODE_FULLSCREEN] = transition_popup_fullscreen,

                                                /* same as transition popup to
                                                 * tiling */
    [WINDOW_MODE_FULLSCREEN][WINDOW_MODE_TILING] = transition_popup_tiling,
    [WINDOW_MODE_FULLSCREEN][WINDOW_MODE_POPUP] = transition_fullscreen_popup,
};

/* Predicts what kind of mode the window should be in. */
window_mode_t predict_window_mode(Window *window)
{
    if (does_window_have_state(window, ewmh._NET_WM_STATE_FULLSCREEN)) {
        return WINDOW_MODE_FULLSCREEN;
    }

    if (window->properties.transient_for != 0) {
        return WINDOW_MODE_POPUP;
    }

    if (window->properties.number_of_types > 0) {
        if (window->properties.types[0] == ewmh._NET_WM_WINDOW_TYPE_NORMAL) {
            return WINDOW_MODE_TILING;
        }
        return WINDOW_MODE_POPUP;
    }

    /* cheat: popup windows have a maximum size */
    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        return WINDOW_MODE_POPUP;
    }

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

    if (window->popup_size.width == 0) {
        if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
            width = window->properties.size_hints.width;
            height = window->properties.size_hints.height;
        } else {
            width = monitor->frame->width * 2 / 3;
            height = monitor->frame->height * 2 / 3;
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
            x = (monitor->frame->width - width) / 2;
            y = (monitor->frame->height - height) / 2;
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

    if ((window->properties.size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        switch (window->properties.size_hints.win_gravity) {
        case XCB_GRAVITY_NORTH_WEST:
            x = monitor->frame->width - width;
            y = 0;
            break;

        case XCB_GRAVITY_NORTH:
            y = 0;
            break;

        case XCB_GRAVITY_NORTH_EAST:
            x = 0;
            y = 0;
            break;
        
        case XCB_GRAVITY_WEST:
            x = monitor->frame->width - width;
            break;

        case XCB_GRAVITY_CENTER:
            x = (monitor->frame->width - width) / 2;
            y = (monitor->frame->height - height) / 2;
            break;

        case XCB_GRAVITY_EAST:
            x = 0;
            break;

        case XCB_GRAVITY_SOUTH_WEST:
            x = monitor->frame->width - width;
            y = monitor->frame->height - height;
            break;

        case XCB_GRAVITY_SOUTH:
            y = monitor->frame->height - height;
            break;

        case XCB_GRAVITY_SOUTH_EAST:
            x = 0;
            y = monitor->frame->width - height;
            break;
        }
    }

    set_window_size(window, x, y, width, height);
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
        set_window_size(window, monitor->frame->x, monitor->frame->y,
                monitor->frame->width, monitor->frame->height);
    }
}

/* Unmaps a window and sets it to hidden without any mode transitioning. */
static void quick_hide(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);
    window->state.is_visible = false;
}

static void transition_tiling_popup(Window *window)
{
    Window *next;

    next = get_next_hidden_window(window);
    if (next != NULL) {
        link_window_and_frame(next, window->frame);

        next->state.is_visible = true;

        xcb_map_window(g_dpy, next->xcb_window);
    } else {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_popup_size(window);
    set_window_above(window);
}

static void transition_tiling_fullscreen(Window *window)
{
    Window *next;

    next = get_next_hidden_window(window);
    if (next != NULL) {
        link_window_and_frame(next, window->frame);

        next->state.is_visible = true;

        xcb_map_window(g_dpy, next->xcb_window);
    } else {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_fullscreen_size(window);
    set_window_above(window);
}

static void transition_popup_tiling(Window *window)
{
    Window *old_window;

    old_window = g_cur_frame->window;

    link_window_and_frame(window, g_cur_frame);

    set_focus_window(window);

    if (old_window != NULL) {
        quick_hide(old_window);
    }
}

static void transition_popup_fullscreen(Window *window)
{
    configure_fullscreen_size(window);
}

static void transition_fullscreen_popup(Window *window)
{
    configure_popup_size(window);
}

/* Changes the window state to given value and reconfigures the window only
 * if the mode changed. */
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
        if (transitions[window->state.mode][mode] != NULL) {
            transitions[window->state.mode][mode](window);
        }
    }

    window->state.previous_mode = window->state.mode;
    window->state.mode = mode;

    synchronize_window_property(window,
            WINDOW_EXTERNAL_PROPERTY_ALLOWED_ACTIONS);
}

/* Show the window by mapping it to the X server. */
void show_window(Window *window)
{
    Window *last, *previous;
    Window *old_window;

    if (window->state.is_visible) {
        return;
    }

    /* assign the first id to the window if it is first mapped */
    if (!window->state.is_mappable) {
        for (last = g_first_window; last->next != NULL; last = last->next) {
            if (last->number + 1 < last->next->number) {
                break;
            }
        }
        window->number = last->number + 1;

        LOG("assigned id %" PRIu32 " to window wrapping %" PRIu32 "\n",
                window->number, window->xcb_window);

        if (last != window) {
            if (window != g_first_window) {
                previous = get_previous_window(window);
                previous->next = window->next;
            } else {
                g_first_window = window->next;
            }
            window->next = last->next;
            last->next = window;
        }

        window->state.is_mappable = true;
    }

    window->state.is_visible = true;

    LOG("showing window with id: %" PRIu32 "\n", window->number);

    xcb_map_window(g_dpy, window->xcb_window);

    switch (window->state.mode) {
    /* the window has to become part of the tiling layout */
    case WINDOW_MODE_TILING:
        old_window = g_cur_frame->window;
        link_window_and_frame(window, g_cur_frame);
        set_focus_window(window);
        if (old_window != NULL) {
            quick_hide(old_window);
        }
        break;

    /* the window has to show as popup window */
    case WINDOW_MODE_POPUP:
        configure_popup_size(window);
        break;

    /* the window has to show as fullscreen window */
    case WINDOW_MODE_FULLSCREEN:
        configure_fullscreen_size(window);
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }
}

/* Hide the window by unmapping it from the X server. */
void hide_window(Window *window)
{
    Window *next;

    if (!window->state.is_visible) {
        return;
    }

    window->state.is_visible = false;

    switch (window->state.mode) {
    /* the window is replaced by another window in the tiling layout */
    case WINDOW_MODE_TILING:
        next = get_next_hidden_window(window);
        if (next != NULL) {
            link_window_and_frame(next, window->frame);

            next->state.is_visible = true;

            xcb_map_window(g_dpy, next->xcb_window);

            set_focus_window(window);
        } else {
            if (window == get_focus_window()) {
                give_someone_else_focus(window);
            }
            window->frame->window = NULL;
            window->frame = NULL;
        }
        break;

    /* need to do nothing here, just unmap and focus a different window */
    case WINDOW_MODE_POPUP:
    case WINDOW_MODE_FULLSCREEN:
        give_someone_else_focus(window);
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        break;
    }

    xcb_unmap_window(g_dpy, window->xcb_window);
}
