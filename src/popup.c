#include <inttypes.h>
#include <signal.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "util.h"

/* The whole purpose of this file is to detect if a window is a popup window
 * and handle a case where a window changes from its window state.
 *
 * Since a change from one state to another one depends on the previous state,
 * the transition system was created with function pointers.
 */

static void transition_hidden_shown(Window *window);
static void transition_hidden_popup(Window *window);

static void transition_shown_hidden(Window *window);
static void transition_shown_popup(Window *window);

static void transition_popup_hidden(Window *window);
static void transition_popup_shown(Window *window);

static void (*transitions[4][4])(Window *window) = {
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_HIDDEN] = NULL,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_SHOWN] = transition_hidden_shown,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_POPUP] = transition_hidden_popup,
    [WINDOW_STATE_HIDDEN][WINDOW_STATE_IGNORE] = NULL,

    [WINDOW_STATE_SHOWN][WINDOW_STATE_HIDDEN] = transition_shown_hidden,
    [WINDOW_STATE_SHOWN][WINDOW_STATE_SHOWN] = NULL,
    [WINDOW_STATE_SHOWN][WINDOW_STATE_POPUP] = transition_shown_popup,
                                            /* same as transition from shown to
                                             * hidden */
    [WINDOW_STATE_SHOWN][WINDOW_STATE_IGNORE] = transition_shown_hidden,

    [WINDOW_STATE_POPUP][WINDOW_STATE_HIDDEN] = transition_popup_hidden,
    [WINDOW_STATE_POPUP][WINDOW_STATE_SHOWN] = transition_popup_shown,
    [WINDOW_STATE_POPUP][WINDOW_STATE_POPUP] = NULL,
                                            /* same as transition from popup to
                                             * hidden */
    [WINDOW_STATE_POPUP][WINDOW_STATE_IGNORE] = transition_popup_hidden,

    [WINDOW_STATE_IGNORE][WINDOW_STATE_HIDDEN] = NULL,
                                            /* same as transition from hidden */
    [WINDOW_STATE_IGNORE][WINDOW_STATE_SHOWN] = transition_hidden_shown,
    [WINDOW_STATE_IGNORE][WINDOW_STATE_POPUP] = transition_hidden_popup,
    [WINDOW_STATE_IGNORE][WINDOW_STATE_IGNORE] = NULL,
};

/* Update the short_title of the window according to the X11 name. */
void update_window_name(Window *window)
{
    xcb_get_property_cookie_t   name_cookie;
    xcb_get_property_reply_t    *name_reply;

    name_cookie = xcb_get_property(g_dpy, 0, window->xcb_window, XCB_ATOM_WM_NAME,
            UTF8_STRING, 0, (sizeof(window->short_title) >> 2) + 1);

    name_reply = xcb_get_property_reply(g_dpy, name_cookie, NULL);
    snprintf((char*) window->short_title, sizeof(window->short_title),
        "%" PRId32 "-%.*s",
            window->number,
            name_reply == NULL ? 0 :
                xcb_get_property_value_length(name_reply),
            name_reply == NULL ? "" :
                (char*) xcb_get_property_value(name_reply));
    free(name_reply);
}

/* Update the size_hints of the window according to the X11 name. */
void update_window_size_hints(Window *window)
{
    xcb_get_property_cookie_t   size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(g_dpy, window->xcb_window,
            XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(g_dpy, size_hints_cookie,
                &window->size_hints, NULL)) {
        memset(&window->size_hints, 0, sizeof(window->size_hints));
    }
}

/* Predicts whether the window should be a popup window. */
unsigned predict_popup(Window *window)
{
    xcb_window_t transient;

    if (!xcb_icccm_get_wm_transient_for_reply(g_dpy,
            xcb_icccm_get_wm_transient_for_unchecked(g_dpy, window->xcb_window),
            &transient, NULL) || transient == 0) {
        if (window->size_hints.min_width != 0 && window->size_hints.max_width != 0 &&
                window->size_hints.min_width == window->size_hints.max_width) {
            return 1;
        }
        return 0;
    }
    return 1;
}

/* Set the window size and position according to the size hints. */
static void configure_popup_size(Window *window)
{
    xcb_screen_t *screen;

    screen = g_screens[g_screen_no];

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE)) {
        g_values[2] = window->size_hints.width;
        g_values[3] = window->size_hints.height;
    } else {
        g_values[2] = screen->width_in_pixels * 2 / 3;
        g_values[3] = screen->height_in_pixels * 2 / 3;
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION)) {
        g_values[0] = window->size_hints.x;
        g_values[1] = window->size_hints.y;
    } else {
        g_values[0] = (screen->width_in_pixels - g_values[2]) / 2;
        g_values[1] = (screen->height_in_pixels - g_values[3]) / 2;
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        g_values[2] = MAX(g_values[2], (uint32_t) window->size_hints.min_width);
        g_values[3] = MAX(g_values[3],
                (uint32_t) window->size_hints.min_height);
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        g_values[2] = MIN(g_values[2], (uint32_t) window->size_hints.max_width);
        g_values[3] = MIN(g_values[3],
                (uint32_t) window->size_hints.max_height);
    }

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)) {
        switch (window->size_hints.win_gravity) {
        case XCB_GRAVITY_NORTH_WEST:
            g_values[0] = screen->width_in_pixels - g_values[2];
            g_values[1] = 0;
            break;

        case XCB_GRAVITY_NORTH:
            g_values[1] = 0;
            break;

        case XCB_GRAVITY_NORTH_EAST:
            g_values[0] = 0;
            g_values[1] = 0;
            break;
        
        case XCB_GRAVITY_WEST:
            g_values[0] = screen->width_in_pixels - g_values[2];
            break;

        case XCB_GRAVITY_CENTER:
            g_values[0] = (screen->width_in_pixels - g_values[2]) / 2;
            g_values[1] = (screen->height_in_pixels - g_values[3]) / 2;
            break;

        case XCB_GRAVITY_EAST:
            g_values[0] = 0;
            break;

        case XCB_GRAVITY_SOUTH_WEST:
            g_values[0] = screen->width_in_pixels - g_values[2];
            g_values[1] = screen->height_in_pixels - g_values[3];
            break;

        case XCB_GRAVITY_SOUTH:
            g_values[1] = screen->height_in_pixels - g_values[3];
            break;

        case XCB_GRAVITY_SOUTH_EAST:
            g_values[0] = 0;
            g_values[1] = screen->height_in_pixels - g_values[3];
            break;
        }
    }

    g_values[4] = XCB_STACK_MODE_ABOVE;

    xcb_configure_window(g_dpy, window->xcb_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);
}

static void transition_hidden_shown(Window *window)
{
    Window *old_window;

    old_window = g_frames[g_cur_frame].window;
    g_frames[g_cur_frame].window = window;

    reload_frame(g_cur_frame);

    xcb_map_window(g_dpy, window->xcb_window);

    if (old_window != NULL) {
        xcb_unmap_window(g_dpy, old_window->xcb_window);

        old_window->state = WINDOW_STATE_HIDDEN;
    }
}

static void transition_hidden_popup(Window *window)
{
    configure_popup_size(window);
    xcb_map_window(g_dpy, window->xcb_window);
}

static void transition_shown_hidden(Window *window)
{
    Frame   frame;
    Window  *next;

    frame = get_frame_of_window(window);
    next = get_next_hidden_window(window);
    if (next != NULL) {
        g_frames[frame].window = next;

        reload_frame(frame);

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
    }
    xcb_unmap_window(g_dpy, window->xcb_window);
}

static void transition_shown_popup(Window *window)
{
    Frame   frame;
    Window  *next;

    frame = get_frame_of_window(window);
    next = get_next_hidden_window(window);
    g_frames[frame].window = next;
    if (next != NULL) {
        reload_frame(frame);

        next->state = WINDOW_STATE_SHOWN;

        xcb_map_window(g_dpy, next->xcb_window);
    }
    configure_popup_size(window);
}

static void transition_popup_hidden(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);
    give_someone_else_focus(window);
}

static void transition_popup_shown(Window *window)
{
    Window *old_window;

    old_window = g_frames[g_cur_frame].window;
    g_frames[g_cur_frame].window = window;
    reload_frame(g_cur_frame);

    if (old_window != NULL) {
        xcb_unmap_window(g_dpy, old_window->xcb_window);

        old_window->state = WINDOW_STATE_HIDDEN;
    }
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

    window->state = state;
}
