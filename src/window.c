#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "window.h"
#include "root_properties.h"
#include "screen.h"
#include "xalloc.h"

/* the first window in the linked list, the list is sorted increasingly
 * with respect to the window number
 */
Window *first_window;

/* the currently focused window */
Window *focus_window;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window)
{
    Window *window;
    xcb_get_window_attributes_cookie_t window_attributes_cookie;
    xcb_get_window_attributes_reply_t *window_attributes;

    window = xcalloc(1, sizeof(*window));
    window->xcb_window = xcb_window;

    /* each window starts with id 0, at the beginning of the linked list */
    window->next = first_window;
    first_window = window;

    /* preserve the event mask of our windows */
    window_attributes_cookie = xcb_get_window_attributes(connection,
            xcb_window);
    window_attributes = xcb_get_window_attributes_reply(connection,
            window_attributes_cookie, NULL);
    if (window_attributes == NULL) {
        general_values[0] = 0;
    } else {
        general_values[0] = window_attributes->your_event_mask;
        free(window_attributes);
    }

    general_values[0] |= XCB_EVENT_MASK_PROPERTY_CHANGE |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes(connection, xcb_window, XCB_CW_EVENT_MASK,
            general_values);

    general_values[0] = 0;
    xcb_configure_window(connection, xcb_window, XCB_CONFIG_WINDOW_BORDER_WIDTH,
            general_values);

    update_all_window_properties(window);
    synchronize_all_window_properties(window);

    LOG("created new window wrapping %" PRIu32 "\n", xcb_window);
    return window;
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(Window *window)
{
    Window *previous;

    hide_window_quickly(window);

    /* remove window from window linked list */
    if (window == first_window) {
        first_window = window->next;
    } else {
        previous = first_window;
        while (previous->next != window) {
            previous = previous->next;
        }
        previous->next = window->next;
    }

    LOG("destroyed window %" PRIu32 "\n", window->number);

    free(window);
}

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height)
{
    LOG("configuring size of window %" PRIu32
            " to: %" PRId32 ", %" PRId32 ", %" PRIu32 ", %" PRIu32 "\n",
            window->number, x, y, width, height);

    window->position.x = x;
    window->position.y = y;
    window->size.width = width;
    window->size.height = height;

    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = width;
    general_values[3] = height;
    xcb_configure_window(connection, window->xcb_window, XCB_CONFIG_SIZE,
            general_values);
}

/* Put the window on top of all other windows. */
void set_window_above(Window *window)
{
    Window *above;

    LOG("setting window %" PRIu32 " above all other windows\n", window->number);

    /* desktop windows are always at the bottom */
    if (does_window_have_type(window, ewmh._NET_WM_WINDOW_TYPE_DESKTOP)) {
        general_values[0] = XCB_STACK_MODE_BELOW;
    } else {
        general_values[0] = XCB_STACK_MODE_ABOVE;
    }
    xcb_configure_window(connection, window->xcb_window,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* put windows that are transient for this window above it */
    for (above = first_window; above != NULL; above = above->next) {
        if (!above->state.is_visible) {
            continue;
        }
        if (above->properties.transient_for == window->xcb_window) {
            xcb_configure_window(connection, window->xcb_window,
                    XCB_CONFIG_WINDOW_STACK_MODE, general_values);
        }
    }
}

/* Get the window before this window in the linked list. */
Window *get_previous_window(Window *window)
{
    Window *prev;

    if (window == NULL) {
        return NULL;
    }

    for (prev = first_window; prev->next != window; prev = prev->next) {
        if (prev->next == NULL) {
            break;
        }
    }
    return prev;
}

/* Get the internal window that has the associated xcb window. */
Window *get_window_of_xcb_window(xcb_window_t xcb_window)
{
    for (Window *window = first_window; window != NULL;
            window = window->next) {
        if (window->xcb_window == xcb_window) {
            return window;
        }
    }
#ifdef DEBUG
    /* omit log message for the root window */
    if (xcb_window != screen->xcb_screen->root) {
        LOG("xcb window %" PRIu32 " is not managed\n", xcb_window);
    }
#endif
    return NULL;
}

/* Removes a window from the focus list. */
void unlink_window_from_focus_list(Window *window)
{
    if (window->previous_focus == window) {
         focus_window = NULL;
    }

    if (window->previous_focus != NULL) {
        window->previous_focus->next_focus = window->next_focus;
    }
    if (window->next_focus != NULL) {
        window->next_focus->previous_focus = window->previous_focus;
    }
    window->previous_focus = NULL;
    window->next_focus = NULL;
}

/* Check if the window accepts input focus. */
bool does_window_accept_focus(Window *window)
{
    return !(window->properties.hints.flags & XCB_ICCCM_WM_HINT_INPUT) ||
            window->properties.hints.input != 0;
}

/* Set the window that is in focus. */
void set_focus_window(Window *window)
{
    if (!does_window_accept_focus(window)) {
        return;
    }

    if (window == focus_window) {
        LOG("window %" PRIu32 " is already focused\n", window->number);
        return;
    }

    if (focus_window != NULL) {
        unlink_window_from_focus_list(window);

        /* relink window into focus list */
        if (focus_window->next_focus != NULL) {
            focus_window->next_focus->previous_focus = window;
        }
        window->previous_focus = focus_window;
        window->next_focus = focus_window->next_focus;
        focus_window->next_focus = window;
    } else {
        window->previous_focus = window;
        window->next_focus = window;
    }

    focus_window = window;
    synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);

    LOG("changed focus to %" PRIu32 "\n", window->number);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            window->xcb_window, XCB_CURRENT_TIME);
}

/* Focuses the window before or after the currently focused window. */
void traverse_focus_chain(int direction)
{
    Window *window;

    if (focus_window == NULL) {
        return;
    }

    if (direction < 0) {
        window = focus_window->previous_focus;
    } else {
        window = focus_window->next_focus;
    }
    
    if (focus_window == window) {
        return;
    }

    focus_window = window;
    synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);

    LOG("changed focus to %" PRIu32 "\n", window->number);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            window->xcb_window, XCB_CURRENT_TIME);

    if (focus_window->frame != NULL) {
        set_focus_frame(focus_window->frame);
    }
}

/* Focuses the window guaranteed but also focuse the frame of the window if it
 * has one.
 */
void set_focus_window_with_frame(Window *window)
{
    if (window->frame != NULL) {
        set_focus_frame(window->frame);
    } else {
        set_focus_window(window);
    }
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
            next = first_window;
        } else {
            next = next->next;
        }
        if (window == next) {
            return NULL;
        }
    } while (!next->state.was_ever_mapped || next->state.is_visible);

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
    } while (!previous->state.was_ever_mapped || previous->state.is_visible);

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
