#include <inttypes.h>

#include "fensterchef.h"
#include "log.h"
#include "xalloc.h"

/* the first window in the linked list */
Window          *g_first_window;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window)
{
    Window      *window;
    Window      *last;

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

    if (predict_popup(window)) {
        set_window_state(window, WINDOW_STATE_POPUP, 1);
    } else {
        set_window_state(window, WINDOW_STATE_SHOWN, 1);
    }

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

/* Get the frame this window is contained in. */
Frame get_frame_of_window(Window *window)
{
    if (window->state != WINDOW_STATE_SHOWN) {
        return (Frame) -1;
    }

    for (Frame frame = 0; frame < g_frame_capacity; frame++) {
        if (g_frames[frame].window == window) {
            return frame;
        }
    }
    return (Frame) -1;
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
void set_focus_window(Window *window)
{
    Window *old_focus;

    old_focus = get_focus_window();
    if (old_focus != NULL) {
        old_focus->focused = 0;
    }

    window->focused = 1;

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, window->xcb_window,
            XCB_CURRENT_TIME);
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
    } while (other->state != WINDOW_STATE_SHOWN &&
            other->state != WINDOW_STATE_POPUP);

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
    } while (next->state != WINDOW_STATE_HIDDEN);

    return next;
}

/* Get a window that is not shown but in the window list coming before
 * the given window. */
Window *get_previous_hidden_window(Window *window)
{
    Window *prev;

    if (window == NULL) {
        return NULL;
    }
    prev = window;
    do {
        prev = get_previous_window(prev);
        if (window == prev) {
            return NULL;
        }
    } while (prev->state != WINDOW_STATE_HIDDEN);

    return prev;
}
