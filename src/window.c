#include <inttypes.h>
#include <string.h>

#include "configuration.h"
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
    xcb_get_window_attributes_cookie_t attributes_cookie;
    xcb_get_window_attributes_reply_t *attributes;
    xcb_get_geometry_cookie_t geometry_cookie;
    xcb_get_geometry_reply_t *geometry;
    xcb_generic_error_t *error;
    Window *window;
    Window *previous;
    window_mode_t mode;

    attributes_cookie = xcb_get_window_attributes(connection, xcb_window);
    geometry_cookie = xcb_get_geometry(connection, xcb_window);

    attributes = xcb_get_window_attributes_reply(connection, attributes_cookie,
            &error);
    if (attributes == NULL) {
        LOG_ERROR("could not get window attributes of %w: %E\n",
                xcb_window, error);
        free(error);
        xcb_discard_reply(connection, geometry_cookie.sequence);
        return NULL;
    }
    /* override redirect is used by windows to indicate that our window manager
     * should not tamper with them, we also check if the class is InputOnly
     * which is not a case we want to handle
     */
    if (attributes->override_redirect ||
            attributes->_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        free(attributes);
        xcb_discard_reply(connection, geometry_cookie.sequence);
        return NULL;
    }
    free(attributes);

    geometry = xcb_get_geometry_reply(connection, geometry_cookie,
            &error);
    if (geometry == NULL) {
        LOG_ERROR("could not get window geometry of %w: %E\n",
                xcb_window, error);
        free(error);
        return NULL;
    }

    /* set the border color */
    general_values[0] = configuration.border.color;
    /* we want to know if the focus changes in the window or if any properties
     * change
     */
    general_values[1] = XCB_EVENT_MASK_PROPERTY_CHANGE |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes(connection, xcb_window,
            XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, general_values);

    window = xcalloc(1, sizeof(*window));

    window->creation_time = time(NULL);
    /* start off with an invalid mode, this gets set below */
    window->state.mode = WINDOW_MODE_MAX;
    window->x = geometry->x;
    window->y = geometry->y;
    window->width = geometry->width;
    window->height = geometry->height;

    free(geometry);

    if (first_window == NULL) {
        first_window = window;
        window->number = FIRST_WINDOW_NUMBER;
    } else {
        previous = first_window;
        if (first_window->number == FIRST_WINDOW_NUMBER) {
            /* find a gap in the window numbers */
            for (; previous->next != NULL; previous = previous->next) {
                if (previous->number + 1 < previous->next->number) {
                    break;
                }
            }
            window->number = previous->number + 1;
            window->next = previous->next;
            previous->next = window;
        } else {
            window->number = FIRST_WINDOW_NUMBER;
            window->next = first_window;
            first_window = window;
        }

        /* put the window at the top of the Z linked list */
        while (previous->above != NULL) {
            previous = previous->above;
        }
        previous->above = window;
        window->below = previous;
    }

    /* initialize the window mode and Z position */
    mode = initialize_window_properties(&window->properties, xcb_window);
    set_window_mode(window, mode);
    update_window_layer(window);

    synchronize_root_property(ROOT_PROPERTY_CLIENT_LIST);

    LOG("created new window %W\n", window);
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
        xcb_kill_client(connection, window->properties.window);
        return;
    }

    /* bake an event for running a protocol on the window */
    event = (xcb_client_message_event_t*) event_data;
    event->response_type = XCB_CLIENT_MESSAGE;
    event->window = window->properties.window;
    event->type = ATOM(WM_PROTOCOLS);
    event->format = 32;
    memset(&event->data, 0, sizeof(event->data));
    event->data.data32[0] = ATOM(WM_DELETE_WINDOW);
    xcb_send_event(connection, false, window->properties.window,
            XCB_EVENT_MASK_NO_EVENT, (char*) event);

    window->state.was_close_requested = true;
    window->state.user_request_close_time = current_time;
}

/* Remove @window from the Z linked list. */
static void unlink_window_from_z_list(Window *window)
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
        LOG_ERROR("destroying window with focus\n");
    }

    unlink_window_from_z_list(window);

    /* remove window from the window linked list */
    if (first_window == window) {
        first_window = first_window->next;
    } else {
        previous = first_window;
        while (previous->next != window) {
            previous = previous->next;
        }
        previous->next = window->next;
    }

    synchronize_root_property(ROOT_PROPERTY_CLIENT_LIST);

    LOG("destroyed window %W\n", window);

    free(window);
}

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int32_t *x, int32_t *y,
        uint32_t width, uint32_t height, uint32_t window_gravity)
{
    switch (window_gravity) {
    /* attach to the top left */
    case XCB_GRAVITY_NORTH_WEST:
        *x = monitor->x;
        *y = monitor->y;
        break;

    /* attach to the top */
    case XCB_GRAVITY_NORTH:
        *y = monitor->y;
        break;

    /* attach to the top right */
    case XCB_GRAVITY_NORTH_EAST:
        *x = monitor->x + monitor->width - width;
        *y = monitor->y;
        break;

    /* attach to the left */
    case XCB_GRAVITY_WEST:
        *x = monitor->x;
        break;

    /* put it into the center */
    case XCB_GRAVITY_CENTER:
        *x = monitor->x + (monitor->width - width) / 2;
        *y = monitor->y + (monitor->height - height) / 2;
        break;

    /* attach to the right */
    case XCB_GRAVITY_EAST:
        *x = monitor->x + monitor->width - width;
        break;

    /* attach to the bottom left */
    case XCB_GRAVITY_SOUTH_WEST:
        *x = monitor->x;
        *y = monitor->y + monitor->height - height;
        break;

    /* attach to the bottom */
    case XCB_GRAVITY_SOUTH:
        *y = monitor->y + monitor->height - height;
        break;

    /* attach to the bottom right */
    case XCB_GRAVITY_SOUTH_EAST:
        *x = monitor->x + monitor->width - width;
        *y = monitor->y + monitor->width - height;
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
    Monitor *monitor;

    /* make sure the window does not become too large or too small */
    width = MIN(width, WINDOW_MAXIMUM_SIZE);
    height = MIN(height, WINDOW_MAXIMUM_SIZE);
    width = MAX(width, WINDOW_MINIMUM_SIZE);
    height = MAX(height, WINDOW_MINIMUM_SIZE);

    /* adjust with the window's specified maximum/minimum size... */
    if ((window->properties.size_hints.flags &
                XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        width = MIN(width,
                (uint32_t) window->properties.size_hints.max_width);
        height = MIN(height,
                (uint32_t) window->properties.size_hints.max_height);
    }
    /* ...but do not maximize tiling windows, it looks quite ugly */
    if (window->state.mode != WINDOW_MODE_TILING) {
        if ((window->properties.size_hints.flags &
                    XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            width = MAX(width,
                    (uint32_t) window->properties.size_hints.min_width);
            height = MAX(height,
                    (uint32_t) window->properties.size_hints.min_height);
        }
    }

    monitor = get_monitor_from_rectangle(x, y, width, height);
    /* place the window so that it is visible */
    if (x + (int32_t) width < monitor->x + WINDOW_MINIMUM_VISIBLE_SIZE) {
        x = monitor->x + WINDOW_MINIMUM_VISIBLE_SIZE - width;
    } else if (x + WINDOW_MINIMUM_VISIBLE_SIZE >=
            monitor->x + (int32_t) monitor->width) {
        x = monitor->x + monitor->width - WINDOW_MINIMUM_VISIBLE_SIZE;
    }
    if (y + (int32_t) height < monitor->y + WINDOW_MINIMUM_VISIBLE_SIZE) {
        y = monitor->y + WINDOW_MINIMUM_VISIBLE_SIZE - height;
    } else if (y + WINDOW_MINIMUM_VISIBLE_SIZE >=
            monitor->y + (int32_t) monitor->height) {
        y = monitor->y + monitor->height - WINDOW_MINIMUM_VISIBLE_SIZE;
    }

    /* do not bother notifying the X server if nothing changed */
    if (window->x == x && window->y == y &&
            window->width == width && window->height == height) {
        LOG("size of window %W remains %R\n", window,
                x, y, width, height);
        return;
    }

    LOG("configuring size of window %W to %R\n", window,
            x, y, width, height);

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;

    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = width;
    general_values[3] = height;
    xcb_configure_window(connection, window->properties.window,
            XCB_CONFIG_SIZE, general_values);
}

/* Get the window below all other windows. */
Window *get_bottom_window(void)
{
    Window *window;

    if (first_window == NULL) {
        return NULL;
    }
    window = first_window;
    while (window->below != NULL) {
        window = window->below;
    }
    while (window != NULL && !window->state.is_visible) {
        window = window->above;
    }
    return window;
}

/* Get the window on top of all other windows. */
Window *get_top_window(void)
{
    Window *window;

    if (first_window == NULL) {
        return NULL;
    }
    window = first_window;
    while (window->above != NULL) {
        window = window->above;
    }
    while (window != NULL && !window->state.is_visible) {
        window = window->below;
    }
    return window;
}

/* Put the window on the best suited Z stack position. */
void update_window_layer(Window *window)
{
    if (window->state.mode == WINDOW_MODE_TILING) {
        Window *bottom;

        /* check if the window is already at the bottom */
        if (window->below == NULL) {
            return;
        }

        LOG("setting window %W below all other windows\n", window);

        general_values[0] = XCB_STACK_MODE_BELOW;
        xcb_configure_window(connection, window->properties.window,
                XCB_CONFIG_WINDOW_STACK_MODE, general_values);

        for (bottom = window; bottom->below != NULL; bottom = bottom->below) {
            /* nothing */
        }
        unlink_window_from_z_list(window);
        bottom->below = window;
        window->above = bottom;
    } else {
        Window *top;

        /* check if the window is already at the top */
        if (window->above == NULL) {
            return;
        }

        LOG("setting window %W above all other windows\n", window);

        general_values[0] = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(connection, window->properties.window,
                XCB_CONFIG_WINDOW_STACK_MODE, general_values);

        for (top = window; top->above != NULL; top = top->above) {
            /* nothing */
        }
        unlink_window_from_z_list(window);
        top->above = window;
        window->below = top;
    }

    /* put windows that are transient for this window above it */
    for (Window *below = window->below; below != NULL; ) {
        if (below->properties.transient_for == window->properties.window) {
            general_values[0] = window->properties.window;
            general_values[1] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(connection, window->properties.window,
                    XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                    general_values);

            unlink_window_from_z_list(below);
            below->above = window->above;
            below->below = window;
            window->above = below;
            below = window->below;
        } else {
            below = below->below;
        }
    }

    synchronize_root_property(ROOT_PROPERTY_CLIENT_LIST);
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
    /* shortcut: only tiling windows are within a frame */
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
        return;
    }

    LOG("focusing window %W\n", window);

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
}
