#include <inttypes.h>

#include "configuration.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "window.h"
#include "root_properties.h"
#include "xalloc.h"

/* the first window in the linked list, the list is sorted increasingly
 * with respect to the window number
 */
Window *first_window;

/* the currently focused window */
Window *focus_window;

/* Create a window struct and add it to the window list. */
Window *create_window(xcb_window_t xcb_window)
{
    Window *window;
    xcb_generic_error_t *error;

    window = xcalloc(1, sizeof(*window));

    /* each window starts with id 0, at the beginning of the linked list */
    window->next = first_window;
    first_window = window;

    /* set the border color and set the event mask for focus events */
    general_values[0] = configuration.border.color;
    general_values[1] = XCB_EVENT_MASK_PROPERTY_CHANGE |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    error = xcb_request_check(connection,
            xcb_change_window_attributes_checked(connection, xcb_window,
            XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, general_values));
    if (error != NULL) {
        LOG_ERROR(error, "could not change window attributes");
        /* still continue */
    }

    init_window_properties(&window->properties, xcb_window);

    set_window_mode(window, predict_window_mode(window), false);

    LOG("created new window wrapping %" PRIu32 "\n", xcb_window);
    return window;
}

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(Window *window)
{
    time_t current_time;
    uint8_t event_data[32];
    xcb_client_message_event_t *event;

    current_time = time(NULL);
    /* if either `WM_DELETE_WINDOW` is not supported or a close was requested
     * twice in a row
     */
    if (!supports_protocol(&window->properties, ATOM(WM_DELETE_WINDOW)) ||
            (window->state.was_close_requested && current_time <=
                window->state.user_request_close_time +
                    REQUEST_CLOSE_MAX_DURATION)) {
        free(xcb_request_check(connection, xcb_kill_client_checked(connection,
                    window->properties.window)));
        return;
    }

    /* bake an event for running a protocol on the window */
    event = (xcb_client_message_event_t*) event_data;
    event->response_type = XCB_CLIENT_MESSAGE;
    event->window = window->properties.window;
    event->type = ATOM(WM_PROTOCOLS);
    event->format = 32;
    event->data.data32[0] = ATOM(WM_DELETE_WINDOW);
    event->data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(connection, false, window->properties.window,
            XCB_EVENT_MASK_NO_EVENT, (char*) event);

    window->state.was_close_requested = true;
    window->state.user_request_close_time = current_time;
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(Window *window)
{
    Window *previous;

    /* really make sure the window is hidden, not sure if this case can ever
     * happen because usually a MapUnnotify event hides the window beforehand
     */
    hide_window_quickly(window);

    /* remove window from the window linked list */
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

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int32_t *x, int32_t *y,
        uint32_t width, uint32_t height, uint32_t window_gravity)
{
    switch (window_gravity) {
    /* attach to the top left */
    case XCB_GRAVITY_NORTH_WEST:
        *x = monitor->position.x;
        *y = monitor->position.y;
        break;

    /* attach to the top */
    case XCB_GRAVITY_NORTH:
        *y = monitor->position.y;
        break;

    /* attach to the top right */
    case XCB_GRAVITY_NORTH_EAST:
        *x = monitor->position.x + monitor->size.width - width;
        *y = monitor->position.y;
        break;

    /* attach to the left */
    case XCB_GRAVITY_WEST:
        *x = monitor->position.x;
        break;

    /* put it into the center */
    case XCB_GRAVITY_CENTER:
        *x = monitor->position.x + (monitor->size.width - width) / 2;
        *y = monitor->position.y + (monitor->size.height - height) / 2;
        break;

    /* attach to the right */
    case XCB_GRAVITY_EAST:
        *x = monitor->position.x + monitor->size.width - width;
        break;

    /* attach to the bottom left */
    case XCB_GRAVITY_SOUTH_WEST:
        *x = monitor->position.x;
        *y = monitor->position.y + monitor->size.height - height;
        break;

    /* attach to the bottom */
    case XCB_GRAVITY_SOUTH:
        *y = monitor->position.y + monitor->size.height - height;
        break;

    /* attach to the bottom right */
    case XCB_GRAVITY_SOUTH_EAST:
        *x = monitor->position.x + monitor->size.width - width;
        *y = monitor->position.y + monitor->size.width - height;
        break;

    /* nothing to do */
    case XCB_GRAVITY_STATIC:
        break;
    }
}

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height)
{
    width = MAX(width, 1);
    height = MAX(height, 1);

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
    xcb_configure_window(connection, window->properties.window, XCB_CONFIG_SIZE,
            general_values);
}

/* Put the window on top of all other windows. */
void set_window_above(Window *window)
{
    Window *above;

    LOG("setting window %" PRIu32 " above all other windows\n", window->number);

    /* desktop windows are always at the bottom */
    if (has_window_type(&window->properties,
                ATOM(_NET_WM_WINDOW_TYPE_DESKTOP))) {
        general_values[0] = XCB_STACK_MODE_BELOW;
    } else {
        general_values[0] = XCB_STACK_MODE_ABOVE;
    }
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* put windows that are transient for this window above it */
    for (above = first_window; above != NULL; above = above->next) {
        if (!above->state.is_visible) {
            continue;
        }
        if (above->properties.transient_for == window->properties.window) {
            general_values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(connection, window->properties.window,
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
        if (window->properties.window == xcb_window) {
            return window;
        }
    }
#ifdef DEBUG
    /* omit log message for the root window */
    if (xcb_window != screen->root) {
        LOG("xcb window %" PRIu32 " is not managed\n", xcb_window);
    }
#endif
    return NULL;
}

/* Checks if @frame contains @window and checks this for all its children. */
static Frame *find_frame_recursively(Frame *frame, const Window *window)
{
    if (frame->window == window) {
        return frame;
    }

    if (frame->left == NULL) {
        return NULL;
    }

    Frame *const find = find_frame_recursively(frame->left, window);
    if (find != NULL) {
        return find;
    }
    
    return find_frame_recursively(frame->right, window);
}

/* Get the frame this window is contained in. */
Frame *get_frame_of_window(const Window *window)
{
    /* only tiling windows are within a frame */
    if (window->state.mode != WINDOW_MODE_TILING) {
        return NULL;
    }

    for (Monitor *monitor = first_monitor; monitor != NULL;
            monitor = monitor->next) {
        Frame *const find = find_frame_recursively(monitor->frame, window);
        if (find != NULL) {
            return find;
        }
    }
    return NULL;
}

/* Removes @window from the focus list. */
void unlink_window_from_focus_list(Window *window)
{
    if (window->previous_focus == window) {
        set_focus_window_primitively(NULL);
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

/* Check if @window accepts input focus. */
bool does_window_accept_focus(Window *window)
{
    if (window->state.mode == WINDOW_MODE_DOCK) {
        return false;
    }

    /* desktop windows are in the background, they are at the bottom and not
     * focusable
     */
    if (has_window_type(&window->properties,
                ATOM(_NET_WM_WINDOW_TYPE_DESKTOP))) {
        return false;
    }

    return !(window->properties.hints.flags & XCB_ICCCM_WM_HINT_INPUT) ||
            window->properties.hints.input != 0;
}

/* Set @focus_window to @window and update the border colors. */
void set_focus_window_primitively(Window *window)
{
    if (focus_window != NULL) {
        general_values[0] = configuration.border.color;
        xcb_change_window_attributes(connection, focus_window->properties.window,
                XCB_CW_BORDER_PIXEL, general_values);
    }

    focus_window = window;
    synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);

    if (window != NULL) {
        general_values[0] = configuration.border.focus_color;
        xcb_change_window_attributes(connection, window->properties.window,
                XCB_CW_BORDER_PIXEL, general_values);

        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
                window->properties.window, XCB_CURRENT_TIME);

        LOG("changed focus to %" PRIu32 "\n", window->number);
    }
}

/* Set the window that is in focus to @window. */
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

    set_focus_window_primitively(window);
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

    /* keep the focus chain intact */
    set_focus_window_primitively(window);

    Frame *const frame = get_frame_of_window(focus_window);
    if (frame != NULL) {
        set_focus_frame(frame);
    }
}

/* Focuses the window guaranteed but also focuse the frame of the window if it
 * has one.
 */
void set_focus_window_with_frame(Window *window)
{
    Frame *const frame = get_frame_of_window(window);
    if (frame != NULL) {
        set_focus_frame(frame);
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
