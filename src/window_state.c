#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "screen.h"
#include "util.h"

/* The whole purpose of this file is to detect the initial state of a window
 * and to handle a case where a window changes its window state.
 *
 * Since a change from one state to another depends on the previous state,
 * the transition system was created with function pointers.
 */

static void transition_hidden_shown(Window *window);
static void transition_hidden_popup(Window *window);
static void transition_hidden_fullscreen(Window *window);

static void transition_shown_hidden(Window *window);
static void transition_shown_popup(Window *window);
static void transition_shown_fullscreen(Window *window);

static void transition_popup_hidden(Window *window);
static void transition_popup_shown(Window *window);
static void transition_popup_fullscreen(Window *window);

static void transition_fullscreen_popup(Window *window);

static void (*transitions[5][5])(Window *window) = {
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_HIDDEN] = NULL,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_SHOWN] = transition_hidden_shown,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_POPUP] = transition_hidden_popup,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_IGNORE] = NULL,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_FULLSCREEN] = transition_hidden_fullscreen,

    [WINDOW_STATE_SHOWN][WINDOW_STATE_HIDDEN] = transition_shown_hidden,
    [WINDOW_STATE_SHOWN][WINDOW_STATE_SHOWN] = NULL,
    [WINDOW_STATE_SHOWN][WINDOW_STATE_POPUP] = transition_shown_popup,
                                            /* same as transition from shown to
                                             * hidden */
    [WINDOW_STATE_SHOWN][WINDOW_STATE_IGNORE] = transition_shown_hidden,
    [WINDOW_STATE_SHOWN][WINDOW_STATE_FULLSCREEN] = transition_shown_fullscreen,

    [WINDOW_STATE_POPUP][WINDOW_STATE_HIDDEN] = transition_popup_hidden,
    [WINDOW_STATE_POPUP][WINDOW_STATE_SHOWN] = transition_popup_shown,
    [WINDOW_STATE_POPUP][WINDOW_STATE_POPUP] = NULL,
                                            /* same as transition from popup to
                                             * hidden */
    [WINDOW_STATE_POPUP][WINDOW_STATE_IGNORE] = transition_popup_hidden,
    [WINDOW_STATE_POPUP][WINDOW_STATE_FULLSCREEN] = transition_popup_fullscreen,

    [WINDOW_STATE_IGNORE][WINDOW_STATE_HIDDEN] = NULL,
                                            /* same transitions from hidden */
    [WINDOW_STATE_IGNORE][WINDOW_STATE_SHOWN] = transition_hidden_shown,
    [WINDOW_STATE_IGNORE][WINDOW_STATE_POPUP] = transition_hidden_popup,
    [WINDOW_STATE_IGNORE][WINDOW_STATE_IGNORE] = NULL,
    [WINDOW_STATE_IGNORE][WINDOW_STATE_FULLSCREEN] = transition_hidden_fullscreen,

                                            /* most transitions are the same as
                                             * for popups */
    [WINDOW_STATE_FULLSCREEN][WINDOW_STATE_HIDDEN] = transition_popup_hidden,
    [WINDOW_STATE_FULLSCREEN][WINDOW_STATE_SHOWN] = transition_popup_shown,
    [WINDOW_STATE_FULLSCREEN][WINDOW_STATE_POPUP] = transition_fullscreen_popup,
    [WINDOW_STATE_FULLSCREEN][WINDOW_STATE_IGNORE] = transition_popup_hidden,
    [WINDOW_STATE_FULLSCREEN][WINDOW_STATE_FULLSCREEN] = NULL,
};

/* Predicts whether the window should be a popup window. */
unsigned predict_window_state(Window *window)
{
    xcb_window_t                transient;
    xcb_get_property_cookie_t   state_cookie;
    xcb_get_property_reply_t    *state_reply;
    xcb_atom_t                  *atoms;
    int                         num_atoms;

    if (!xcb_icccm_get_wm_transient_for_reply(g_dpy,
            xcb_icccm_get_wm_transient_for_unchecked(g_dpy, window->xcb_window),
            &transient, NULL) || transient == 0) {
        if ((window->size_hints.flags & (XCB_ICCCM_SIZE_HINT_P_MAX_SIZE |
                        XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) ==
                (XCB_ICCCM_SIZE_HINT_P_MAX_SIZE |
                        XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            if (window->size_hints.min_width == window->size_hints.max_width) {
                return WINDOW_STATE_POPUP;
            }
        }

        state_cookie = xcb_get_property(g_dpy, 0, window->xcb_window,
                g_ewmh._NET_WM_STATE, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
        state_reply = xcb_get_property_reply(g_dpy, state_cookie, NULL);
        if (state_reply == NULL) {
            return WINDOW_STATE_SHOWN;
        }

        atoms = xcb_get_property_value(state_reply);
        num_atoms = xcb_get_property_value_length(state_reply);
        if (atoms != NULL && num_atoms > 0) {
            num_atoms /= state_reply->format / 8;
            for (int i = 0; i < num_atoms; i++) {
                if (atoms[i] == g_ewmh._NET_WM_STATE_FULLSCREEN) {
                    free(state_reply);
                    return WINDOW_STATE_FULLSCREEN;
                }
            }
        }

        free(state_reply);
        return WINDOW_STATE_SHOWN;
    }
    return WINDOW_STATE_POPUP;
}

/* Set the window size and position according to the size hints. */
static void configure_popup_size(Window *window)
{
    Monitor *monitor;
    int32_t x, y;
    uint32_t width, height;

    monitor = get_monitor_from_rectangle(window->x, window->y,
            window->width, window->height);

    if (window->popup_width == 0) {
        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE)) {
            width = window->size_hints.width;
            height = window->size_hints.height;
        } else {
            width = monitor->frame->width * 2 / 3;
            height = monitor->frame->height * 2 / 3;
        }

        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION)) {
            x = window->size_hints.x;
            y = window->size_hints.y;
        } else {
            x = (monitor->frame->width - width) / 2;
            y = (monitor->frame->height - height) / 2;
        }

        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            width = MAX(g_values[2], (uint32_t) window->size_hints.min_width);
            height = MAX(height,
                    (uint32_t) window->size_hints.min_height);
        }

        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
            width = MIN(g_values[2], (uint32_t) window->size_hints.max_width);
            height = MIN(height,
                    (uint32_t) window->size_hints.max_height);
        }

        window->popup_width = width;
        window->popup_height = height;
    } else {
        width = window->popup_width;
        height = window->popup_width;
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        switch (window->size_hints.win_gravity) {
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
    set_window_above(window);
}

/* Sets the position and size of the window to fullscreen. */
static void configure_fullscreen_size(Window *window)
{
    Monitor *monitor;

    monitor = get_monitor_from_rectangle(window->x, window->y,
            window->width, window->height);
    set_window_size(window, monitor->frame->x, monitor->frame->y,
            monitor->frame->width, monitor->frame->height);
    set_window_above(window);
}

/* Unmaps a window and sets it to hidden without any state transitioning. */
static void quick_hide(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);
    window->prev_state = window->state;
    window->state = WINDOW_STATE_HIDDEN;
    window->forced_state = 1;
}

static void transition_hidden_shown(Window *window)
{
    Window *old_window;

    old_window = g_cur_frame->window;

    /* mechanism for off screen frames (not yet used) */
    if (window->frame != NULL) {
        window->frame->window = NULL;
    }

    link_window_and_frame(window, g_cur_frame);

    xcb_map_window(g_dpy, window->xcb_window);

    set_focus_window(window);

    if (old_window != NULL) {
        quick_hide(old_window);
    }
}

static void transition_hidden_popup(Window *window)
{
    /* mechanism for off screen frames (not yet used) */
    if (window->frame != NULL) {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_popup_size(window);
    xcb_map_window(g_dpy, window->xcb_window);
}

static void transition_hidden_fullscreen(Window *window)
{
    /* mechanism for off screen frames (not yet used) */
    if (window->frame != NULL) {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_fullscreen_size(window);
    xcb_map_window(g_dpy, window->xcb_window);
}

static void transition_shown_hidden(Window *window)
{
    Window *next;

    next = get_next_hidden_window(window);
    if (next != NULL) {
        link_window_and_frame(next, window->frame);

        next->state = WINDOW_STATE_SHOWN;

        xcb_map_window(g_dpy, next->xcb_window);

        if (window->focused) {
            window->focused = 0;
            next->focused = 1;
            xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                    next->xcb_window, XCB_CURRENT_TIME);
        }
    } else if (window->focused) {
        give_someone_else_focus(window);
        window->frame->window = NULL;
        window->frame = NULL;
    }

    xcb_unmap_window(g_dpy, window->xcb_window);
}

static void transition_shown_popup(Window *window)
{
    Window *next;

    next = get_next_hidden_window(window);
    if (next != NULL) {
        link_window_and_frame(next, window->frame);

        next->state = WINDOW_STATE_SHOWN;

        xcb_map_window(g_dpy, next->xcb_window);
    } else {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_popup_size(window);
}

static void transition_shown_fullscreen(Window *window)
{
    Window *next;

    next = get_next_hidden_window(window);
    if (next != NULL) {
        link_window_and_frame(next, window->frame);

        next->state = WINDOW_STATE_SHOWN;

        xcb_map_window(g_dpy, next->xcb_window);
    } else {
        window->frame->window = NULL;
        window->frame = NULL;
    }

    configure_fullscreen_size(window);
}

static void transition_popup_hidden(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);
    give_someone_else_focus(window);
}

static void transition_popup_shown(Window *window)
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
 * if the state changed. */
void set_window_state(Window *window, unsigned state, unsigned force)
{
    if (window->state == state ||
            (window->forced_state && !force)) {
        return;
    }

    window->forced_state = force;
    if (transitions[window->state][state] != NULL) {
        transitions[window->state][state](window);
    }

    LOG("state of window %" PRIu32 " changed to %u\n",
            window->number, state);

    window->prev_state = window->state;
    window->state = state;
}
