#include <inttypes.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"
#include "xalloc.h"

/* the first window in the linked list, the list is sorted increasingly
 * with respect to the window number
 */
Window *g_first_window;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window)
{
    Window *window;
    Window *last;

    window = xcalloc(1, sizeof(*window));
    window->xcb_window = xcb_window;

    if (g_first_window == NULL) {
        g_first_window = window;
        window->number = FIRST_WINDOW_NUMBER;
    } else {
        for (last = g_first_window; last->next != NULL; last = last->next) {
            if (last->number + 1 != last->next->number) {
                break;
            }
        }
        window->number = last->number + 1;

        while (last->next != NULL) {
            last = last->next;
        }
        last->next = window;
    }

    g_values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_dpy, xcb_window,
        XCB_CW_EVENT_MASK, g_values);

    g_values[0] = 0;
    xcb_configure_window(g_dpy, xcb_window, XCB_CONFIG_WINDOW_BORDER_WIDTH,
            g_values);

    update_window_name(window);
    update_window_size_hints(window);
    update_window_wm_hints(window);

    set_window_state(window, predict_window_state(window), 0);

    LOG("created new window %" PRIu32 "\n", window->number);
    return window;
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(Window *window)
{
    Window *prev;

    if (window == g_first_window) {
        g_first_window = window->next;
    } else {
        prev = g_first_window;
        while (prev->next != window) {
            prev = prev->next;
        }
        prev->next = window->next;
    }

    LOG("destroyed window %" PRIu32 "\n", window->number);

    free(window);
}

/* Update the short_title of the window. */
void update_window_name(Window *window)
{
    xcb_get_property_cookie_t           name_cookie;
    xcb_ewmh_get_utf8_strings_reply_t   data;

    name_cookie = xcb_ewmh_get_wm_name(&g_ewmh, window->xcb_window);

    xcb_ewmh_get_wm_name_reply(&g_ewmh, name_cookie, &data, NULL);

    snprintf((char*) window->short_title, sizeof(window->short_title),
        "%" PRId32 "-%.*s",
            window->number,
            (int) MIN(data.strings_len, (uint32_t) INT_MAX), data.strings);

    xcb_ewmh_get_utf8_strings_reply_wipe(&data);
}

/* Update the size_hints of the window. */
void update_window_size_hints(Window *window)
{
    xcb_get_property_cookie_t size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(g_dpy, window->xcb_window,
            XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(g_dpy, size_hints_cookie,
                &window->size_hints, NULL)) {
        window->size_hints.flags = 0;
    }
}

/* Update the wm_hints of the window. */
void update_window_wm_hints(Window *window)
{
    xcb_get_property_cookie_t wm_hints_cookie;

    wm_hints_cookie = xcb_icccm_get_wm_hints(g_dpy, window->xcb_window);
    if (!xcb_icccm_get_wm_hints_reply(g_dpy, wm_hints_cookie,
                &window->wm_hints, NULL)) {
        window->wm_hints.flags = 0;
    }
}

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height)
{
    window->position.x = x;
    window->position.y = y;
    window->size.width = width;
    window->size.height = height;

    g_values[0] = x;
    g_values[1] = y;
    g_values[2] = width;
    g_values[3] = height;
    xcb_configure_window(g_dpy, window->xcb_window, XCB_CONFIG_SIZE, g_values);
}

/* Put the window on top of all other windows. */
void set_window_above(Window *window)
{
    g_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, window->xcb_window,
            XCB_CONFIG_WINDOW_STACK_MODE, g_values);
}

/* Get the window before this window in the linked list. */
Window *get_previous_window(Window *window)
{
    Window *prev;

    if (window == NULL) {
        return NULL;
    }

    for (prev = g_first_window; prev->next != window; prev = prev->next) {
        if (prev->next == NULL) {
            break;
        }
    }
    return prev;
}

/* Get the internal window that has the associated xcb window. */
Window *get_window_of_xcb_window(xcb_window_t xcb_window)
{
    for (Window *window = g_first_window; window != NULL;
            window = window->next) {
        if (window->xcb_window == xcb_window) {
            return window;
        }
    }
    LOG("xcb window %" PRIu32 " is not managed\n", xcb_window);
    return NULL;
}

/* Get the currently focused window. */
Window *get_focus_window(void)
{
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        if (w->focused) {
            return w;
        }
    }
    return NULL;
}

/* Set the window that is in focus. */
int set_focus_window(Window *window)
{
    Window *old_focus;

    if ((window->wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT) &&
            window->wm_hints.input == 0) {
        return 1;
    }

    old_focus = get_focus_window();
    if (old_focus != NULL) {
        old_focus->focused = 0;
    }

    window->focused = 1;

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, window->xcb_window,
            XCB_CURRENT_TIME);
    return 0;
}

/* Gives any window different from given window focus. */
void give_someone_else_focus(Window *window)
{
    Window *other;

    window->focused = 0;
    /* TODO: use a focus stack? */
    do {
        if (window->next == NULL) {
            other = g_first_window;
        } else {
            other = window->next;
        }
        if (other == window) {
            return;
        }
    } while (other->state.current != WINDOW_STATE_SHOWN &&
            other->state.current != WINDOW_STATE_POPUP);

    other->focused = 1;

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, window->xcb_window,
            XCB_CURRENT_TIME);
}

/* Get a window that is not shown but in the window list coming after
 * the given window. */
Window *get_next_hidden_window(Window *window)
{
    Window *next;

    if (window == NULL) {
        return NULL;
    }
    next = window;
    do {
        if (next->next == NULL) {
            next = g_first_window;
        } else {
            next = next->next;
        }
        if (window == next) {
            return NULL;
        }
    } while (next->state.current != WINDOW_STATE_HIDDEN);

    return next;
}

/* Get a window that is not shown but in the window list coming before
 * the given window. */
Window *get_previous_hidden_window(Window *window)
{
    Window *previous;

    if (window == NULL) {
        return NULL;
    }

    previous = window;
    do {
        previous = get_previous_window(previous);
        if (window == previous) {
            return NULL;
        }
    } while (previous->state.current != WINDOW_STATE_HIDDEN);

    return previous;
}

/* Puts a window into a frame and matches its size. */
void link_window_and_frame(Window *window, Frame *frame)
{
    if (window->frame != NULL) {
        window->frame->window = NULL;
    }
    if (frame->window != NULL) {
        frame->window->frame = NULL;
    }

    window->frame = frame;
    frame->window = window;
    set_window_size(window, frame->x, frame->y, frame->width, frame->height);
}
