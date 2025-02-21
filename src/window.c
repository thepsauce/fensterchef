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

/* the last window in the taken window linked list */
Window *last_taken_window;

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

    /* set the initial window size */
    general_values[0] = configuration.border.size;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);

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

/* Remove @window from the Z linked list. */
void unlink_window_from_z_list(Window *window)
{
    if (window->below != NULL) {
        window->below->above = window->above;
    }
    if (window->above != NULL) {
        window->above->below = window->below;
    }

    window->below = NULL;
    window->above = NULL;
}

/* Remove @window from the taken linked list. */
void unlink_window_from_taken_list(Window *window)
{
    Window *previous;

    if (window == last_taken_window) {
        last_taken_window = last_taken_window->previous_taken;
    } else {
        previous = last_taken_window;
        while (previous != NULL && previous->previous_taken != window) {
            previous = previous->previous_taken;
        }
        if (previous != NULL) {
            previous->previous_taken = window->previous_taken;
        }
    }
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(Window *window)
{
    Window *previous;

    /* really make sure the window is hidden, not sure if this case can ever
     * happen because usually a MapUnnotify event hides the window beforehand
     */
    hide_window_abruptly(window);

    /* exceptional state, this should never happen */
    if (window == focus_window) {
        focus_window = NULL;
        LOG_ERROR(NULL, "destroying window with focus\n");
    }

    unlink_window_from_z_list(window);
    unlink_window_from_taken_list(window);

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
    /* make sure the window does not become too large or too small */
    width = MAX(width, WINDOW_MINIMUM_SIZE);
    height = MAX(height, WINDOW_MINIMUM_SIZE);
    width = MIN(width, WINDOW_MAXIMUM_SIZE);
    height = MIN(height, WINDOW_MAXIMUM_SIZE);

    /* place the window so that it is visible */
    if (x + (int32_t) width < WINDOW_MINIMUM_VISIBLE_SIZE) {
        x = WINDOW_MINIMUM_VISIBLE_SIZE - width;
    } else if (x + WINDOW_MINIMUM_VISIBLE_SIZE > screen->width_in_pixels) {
        x = screen->width_in_pixels - WINDOW_MINIMUM_VISIBLE_SIZE;
    }
    if (y + (int32_t) height < WINDOW_MINIMUM_VISIBLE_SIZE) {
        y = WINDOW_MINIMUM_VISIBLE_SIZE - height;
    } else if (y + WINDOW_MINIMUM_VISIBLE_SIZE > screen->height_in_pixels) {
        y = screen->height_in_pixels - WINDOW_MINIMUM_VISIBLE_SIZE;
    }

    LOG("configuring size of window %" PRIu32
            " to: %" PRId32 ", %" PRId32 ", %" PRIu32 ", %" PRIu32 "\n",
            window->number, x, y, width, height);

    /* do not bother notifying the X server if nothing changed */
    if (window->position.x == x && window->position.y == y &&
            window->size.width == width && window->size.height == height) {
        return;
    }

    window->position.x = x;
    window->position.y = y;
    window->size.width = width;
    window->size.height = height;

    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = width;
    general_values[3] = height;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_SIZE, general_values);
}

/* Link the window into the Z linked list. */
static void link_window_into_z_list(Window *window, Window *top)
{
    /* make sure `top` is actually the top */
    while (top->above != NULL) {
        top = top->above;
    }

    /* put @window above @top */
    window->below = top;
    top->above = window;

    /* reflect this change in the X server */
    general_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);
}

/* Put the window on top of all other windows. */
void set_window_above(Window *window)
{
    Window *top;
    Window *below, *next_below;

    LOG("setting window %" PRIu32 " above all other windows\n", window->number);

    /* check if the window is already the top window */
    if (window->above == NULL) {
        LOG("the window is already above all other windows\n");
        return;
    }

    /* put window at the top in the Z stack */
    top = window->above;
    unlink_window_from_z_list(window);
    link_window_into_z_list(window, top);

    /* put windows that are transient for this window above it */
    for (below = window->below; below != NULL; below = next_below) {
        next_below = below->below;
        if (below->properties.transient_for == window->properties.window) {
            unlink_window_from_z_list(below);
            link_window_into_z_list(below, window);
        }
    }
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

static void lose_focus(Window *window)
{
    if (window == NULL) {
        return;
    }

    general_values[0] = configuration.border.color;
    xcb_change_window_attributes(connection,
            window->properties.window,
            XCB_CW_BORDER_PIXEL, general_values);
}

/* Set the window that is in focus to @window. */
void set_focus_window(Window *window)
{
    if (window == NULL) {
        lose_focus(focus_window);
        focus_window = NULL;
        LOG("removed focus from all windows\n");
        return;
    }

    LOG("focusing window %" PRIu32 "\n", window->number);

    if (!does_window_accept_focus(window)) {
        LOG("the window can not be focused\n");
        return;
    }

    if (window == focus_window) {
        LOG("the window is already focused\n");
        return;
    }

    lose_focus(focus_window);

    focus_window = window;
    synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);

    general_values[0] = configuration.border.focus_color;
    xcb_change_window_attributes(connection, window->properties.window,
            XCB_CW_BORDER_PIXEL, general_values);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            window->properties.window, XCB_CURRENT_TIME);
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
