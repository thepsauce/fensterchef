#include <inttypes.h>
#include <string.h>

#include "configuration.h"
#include "event.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "window.h"
#include "window_properties.h"
#include "xalloc.h"

/* the number of all windows within the linked list, this value is kept up to
 * date through `create_window()` and `destroy_window()`
 */
uint32_t Window_count;

/* the window that was created before any other */
Window *Window_oldest;

/* the window at the bottom of the Z stack */
Window *Window_bottom;

/* the window at the top of the Z stack */
Window *Window_top;

/* the first window in the number linked list */
Window *Window_first;

/* the currently focused window */
Window *Window_focus;

/* Increment the reference count of the window. */
inline void reference_window(Window *window)
{
    window->reference_count++;
}

/* Decrement the reference count of the window and free @window when it reaches
 * 0.
 */
inline void dereference_window(Window *window)
{
    window->reference_count--;
    if (window->reference_count == 0) {
        free(window);
    }
}

/* Find where in the number linked list a gap is.
 *
 * @return NULL when the window should be inserted before the first window.
 */
static inline Window *find_number_gap(void)
{
    Window *previous;

    /* if the first window has window number greater than the first number that
     * means there is space at the front
     */
    if (Window_first->number > (uint32_t)
            configuration.assignment.first_window_number) {
        return NULL;
    }

    previous = Window_first;
    /* find the first window with a higher number than
     * `first_window_number`
     */
    for (; previous->next != NULL; previous = previous->next) {
        if (previous->next->number > (uint32_t)
                configuration.assignment.first_window_number) {
            break;
        }
    }
    /* find a gap in the window numbers */
    for (; previous->next != NULL; previous = previous->next) {
        if (previous->number + 1 < previous->next->number) {
            break;
        }
    }
    return previous;
}

/* Find the window after which a new window with given @number should be
 * inserted
 *
 * @return NULL when the window should be inserted before the first window.
 */
static inline Window *find_window_number(uint32_t number)
{
    Window *previous;

    if (Window_first == NULL || Window_first->number > number) {
        return NULL;
    }

    previous = Window_first;
    /* find a gap in the window numbers */
    for (; previous->next != NULL; previous = previous->next) {
        if (previous->next->number > number) {
            break;
        }
    }
    return previous;
}

/* Create a window struct and add it to the window list. */
Window *create_window(xcb_window_t xcb_window,
        struct configuration_association *association)
{
    xcb_get_window_attributes_cookie_t attributes_cookie;
    xcb_get_geometry_cookie_t geometry_cookie;
    xcb_generic_error_t *error;
    xcb_get_window_attributes_reply_t *attributes;
    xcb_get_geometry_reply_t *geometry;
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

        /* check if the window holds a command */
        char *const command = get_fensterchef_command_property(xcb_window);
        if (command != NULL) {
            struct configuration configuration;

            LOG("window %#" PRIx32 " has command: %s\n", xcb_window, command);

            if (load_configuration(command, &configuration, false) == OK) {
                LOG("doing its actions: %A\n",
                        configuration.startup.number_of_actions,
                        configuration.startup.actions);
                for (uint32_t i = 0;
                        i < configuration.startup.number_of_actions;
                        i++) {
                    do_action(&configuration.startup.actions[i], Window_focus);
                }
                clear_configuration(&configuration);
            }

            free(command);

            /* signal to the window that we executed its command and it can go
             * now
             */
            xcb_delete_property(connection, xcb_window,
                    ATOM(FENSTERCHEF_COMMAND));
        }
        return NULL;
    }

    geometry = xcb_get_geometry_reply(connection, geometry_cookie,
            &error);
    if (geometry == NULL) {
        LOG_ERROR("could not get window geometry of %w: %E\n",
                xcb_window, error);
        free(attributes);
        free(error);
        return NULL;
    }

    /* set the initial border color */
    general_values[0] = configuration.border.color;
    /* we want to know if if any properties change */
    general_values[1] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(connection, xcb_window,
            XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, general_values);

    window = xcalloc(1, sizeof(*window));

    window->reference_count = 1;
    window->client.id = xcb_window;
    window->client.x = geometry->x;
    window->client.y = geometry->x;
    window->client.width = geometry->width;
    window->client.height = geometry->height;
    window->client.background_color = attributes->backing_pixel;
    window->client.border_color = configuration.border.color;
    /* check if the window is already mapped on the X server */
    if (attributes->map_state != XCB_MAP_STATE_UNMAPPED) {
        window->client.is_mapped = true;
    }

    free(geometry);
    free(attributes);

    /* start off with an invalid mode, this gets set below */
    window->state.mode = WINDOW_MODE_MAX;
    window->x = window->client.x;
    window->y = window->client.y;
    window->width = window->client.width;
    window->height = window->client.height;
    window->border_color = window->client.border_color;

    /* get the initial mode and set the window number */
    mode = initialize_window_properties(window, association);
    window->number = association->number;

    /* link into the Z, age and number linked lists */
    if (Window_first == NULL) {
        Window_oldest = window;
        Window_bottom = window;
        Window_top = window;
        Window_first = window;
        if (window->number == 0) {
            window->number = configuration.assignment.first_window_number;
        }
    } else {
        previous = Window_first;
        if (window->number == 0) {
            previous = find_number_gap();
        } else {
            previous = find_window_number(window->number);
        }

        if (previous == NULL) {
            window->next = Window_first;
            Window_first = window;
            if (window->number == 0) {
                window->number = configuration.assignment.first_window_number;
            }
        } else {
            if (window->number == 0) {
                if (previous->number < (uint32_t)
                        configuration.assignment.first_window_number) {
                    window->number =
                        configuration.assignment.first_window_number;
                } else {
                    window->number = previous->number + 1;
                }
            }
            window->next = previous->next;
            previous->next = window;
        }

        /* put the window at the top of the Z linked list */
        window->below = Window_top;
        Window_top->above = window;
        Window_top = window;

        /* put the window into the age linked list */
        previous = Window_oldest;
        while (previous->newer != NULL) {
            previous = previous->newer;
        }
        previous->newer = window;
    }

    /* new window is now in the list */
    Window_count++;

    /* initialize the window mode and Z position */
    set_window_mode(window, mode);
    update_window_layer(window);

    has_client_list_changed = true;

    /* grab the buttons for this window */
    grab_configured_buttons(xcb_window);

    LOG("created new window %W\n", window);
    return window;
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

    if (window == Window_bottom) {
        Window_bottom = window->above;
    }
    if (window == Window_top) {
        Window_top = window->below;
    }

    window->above = NULL;
    window->below = NULL;

    has_client_list_changed = true;
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(Window *window)
{
    Frame *frame;
    Window *previous;

    /* really make sure the window is hidden, not sure if this case can ever
     * happen because usually a MapUnnotify event hides the window beforehand
     */
    hide_window_abruptly(window);

    /* exceptional state, this should never happen */
    if (window == Window_focus) {
        Window_focus = NULL;
        LOG_ERROR("destroying window with focus\n");
    }

    /* this should also never happen but we check just in case */
    frame = get_frame_of_window(window);
    if (frame != NULL) {
        frame->window = NULL;
        LOG_ERROR("window being destroyed is still within a frame\n");
    }

    LOG("destroying window %W\n", window);

    /* remove from the z linked list */
    unlink_window_from_z_list(window);

    /* remove from the age linked list */
    if (Window_oldest == window) {
        Window_oldest = Window_oldest->newer;
    } else {
        previous = Window_oldest;
        while (previous->newer != window) {
            previous = previous->newer;
        }
        previous->newer = window->newer;
    }

    /* remove from the number linked list */
    if (Window_first == window) {
        Window_first = Window_first->next;
    } else {
        previous = Window_first;
        while (previous->next != window) {
            previous = previous->next;
        }
        previous->next = window->next;
    }

    /* window is gone from the list now */
    Window_count--;

    has_client_list_changed = true;

    /* setting the id to None marks the window as destroyed */
    window->client.id = XCB_NONE;
    free(window->name);
    free(window->protocols);
    free(window->states);

    dereference_window(window);
}

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(Window *window)
{
    time_t current_time;
    char event_data[32];
    xcb_client_message_event_t *event;

    current_time = time(NULL);
    /* if either `WM_DELETE_WINDOW` is not supported or a close was requested
     * twice in a row
     */
    if (!supports_protocol(window, ATOM(WM_DELETE_WINDOW)) ||
            (window->state.was_close_requested && current_time <=
                window->state.user_request_close_time +
                    REQUEST_CLOSE_MAX_DURATION)) {
        xcb_kill_client(connection, window->client.id);
        return;
    }

    /* bake an event for running a protocol on the window */
    event = (xcb_client_message_event_t*) event_data;
    event->response_type = XCB_CLIENT_MESSAGE;
    event->window = window->client.id;
    event->type = ATOM(WM_PROTOCOLS);
    event->format = 32;
    memset(&event->data, 0, sizeof(event->data));
    event->data.data32[0] = ATOM(WM_DELETE_WINDOW);
    xcb_send_event(connection, false, window->client.id,
            XCB_EVENT_MASK_NO_EVENT, event_data);

    window->state.was_close_requested = true;
    window->state.user_request_close_time = current_time;
}

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int32_t *x, int32_t *y,
        uint32_t width, uint32_t height, xcb_gravity_t gravity)
{
    switch (gravity) {
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
    default:
        break;
    }
}

/* Get the minimum size the window can have. */
void get_minimum_window_size(const Window *window, Size *size)
{
    uint32_t width = 0, height = 0;

    if (window->state.mode != WINDOW_MODE_TILING) {
        if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            width = window->size_hints.min_width;
            height = window->size_hints.min_height;
        }
    }
    size->width = MAX(width, WINDOW_MINIMUM_SIZE);
    size->height = MAX(height, WINDOW_MINIMUM_SIZE);
}

/* Get the maximum size the window can have. */
void get_maximum_window_size(const Window *window, Size *size)
{
    uint32_t width = UINT32_MAX, height = UINT32_MAX;

    if ((window->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        width = window->size_hints.max_width;
        height = window->size_hints.max_height;
    }
    size->width = MIN(width, WINDOW_MAXIMUM_SIZE);
    size->height = MIN(height, WINDOW_MAXIMUM_SIZE);
}

/* Move the window such that it is in bounds of the screen. */
void place_window_in_bounds(Window *window)
{
    /* do not move dock windows */
    if (window->state.mode == WINDOW_MODE_DOCK) {
        return;
    }

    /* make the window horizontally visible */
    if (window->x + (int32_t) window->width < WINDOW_MINIMUM_VISIBLE_SIZE) {
        window->x = WINDOW_MINIMUM_VISIBLE_SIZE - window->width;
    } else if (window->x + WINDOW_MINIMUM_VISIBLE_SIZE >=
            (int32_t) screen->width_in_pixels) {
        window->x = screen->width_in_pixels - WINDOW_MINIMUM_VISIBLE_SIZE;
    }
    /* make the window vertically visible */
    if (window->y + (int32_t) window->height < WINDOW_MINIMUM_VISIBLE_SIZE) {
        window->y = WINDOW_MINIMUM_VISIBLE_SIZE - window->height;
    } else if (window->y + WINDOW_MINIMUM_VISIBLE_SIZE >=
            (int32_t) screen->height_in_pixels) {
        window->y = screen->height_in_pixels - WINDOW_MINIMUM_VISIBLE_SIZE;
    }
}

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height)
{
    Size minimum, maximum;

    get_minimum_window_size(window, &minimum);
    get_maximum_window_size(window, &maximum);

    /* make sure the window does not become too large or too small */
    width = MIN(width, maximum.width);
    height = MIN(height, maximum.height);
    width = MAX(width, minimum.width);
    height = MAX(height, minimum.height);

    if (window->state.mode == WINDOW_MODE_FLOATING) {
        window->floating.x = x;
        window->floating.y = y;
        window->floating.width = width;
        window->floating.height = height;
    }

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;

    place_window_in_bounds(window);
}

/* Links the window into the z linked list at a specific place and synchronizes
 * this change with the X server.
 *
 * The window will be inserted after @below or at the bottom if @below is NULL.
 */
static void link_window_into_z_list(Window *below, Window *window)
{
    uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;

    /* link onto the bottom of the Z linked list */
    if (below == NULL) {
        LOG("putting %W at the bottom of the stack\n", window);

        if (Window_bottom != NULL) {
            Window_bottom->below = window;
            window->above = Window_bottom;
        } else {
            Window_top = window;
        }
        Window_bottom = window;

        general_values[0] = XCB_STACK_MODE_BELOW;
    /* link it above `below` */
    } else {
        LOG("putting %W above %W\n", window, below);

        if (below->above != NULL) {
            below->above->below = window;
            window->above = below->above;
        }

        below->above = window;
        window->below = below;

        if (below == Window_top) {
            Window_top = window;
        }

        general_values[0] = below->client.id;
        general_values[1] = XCB_STACK_MODE_ABOVE;
        mask |= XCB_CONFIG_WINDOW_SIBLING;
    }

    xcb_configure_window(connection, window->client.id, mask, general_values);

    has_client_list_changed = true;
}

/* Put the window on the best suited Z stack position. */
void update_window_layer(Window *window)
{
    Window *below = NULL;

    unlink_window_from_z_list(window);

    switch (window->state.mode) {
    /* if there are desktop windows, put the window on top of the desktop
     * windows otherwise put it at the bottom
     */
    case WINDOW_MODE_TILING:
        if (Window_bottom != NULL &&
                Window_bottom->state.mode == WINDOW_MODE_DESKTOP) {
            below = Window_bottom;
            while (below->above != NULL &&
                    below->state.mode == WINDOW_MODE_DESKTOP) {
                below = below->above;
            }
        }
        break;

    /* put the window at the top */
    case WINDOW_MODE_FLOATING:
    case WINDOW_MODE_FULLSCREEN:
    case WINDOW_MODE_DOCK:
        below = Window_top;
        break;

    /* put the window at the bottom */
    case WINDOW_MODE_DESKTOP:
        break;

    /* not a real window mode */
    case WINDOW_MODE_MAX:
        return;
    }

    link_window_into_z_list(below, window);

    /* put windows that are transient for this window above it */
    for (below = window->below; below != NULL; below = below->below) {
        if (below->transient_for == window->client.id) {
            link_window_into_z_list(window, below);
            below = window;
        }
    }
}

/* Get the internal window that has the associated xcb window. */
Window *get_window_of_xcb_window(xcb_window_t xcb_window)
{
    for (Window *window = Window_first; window != NULL;
            window = window->next) {
        if (window->client.id == xcb_window) {
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

    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        Frame *const find = find_frame_recursively(monitor->frame, window);
        if (find != NULL) {
            return find;
        }
    }
    return NULL;
}

/* Check if @window accepts input focus. */
bool is_window_focusable(Window *window)
{
    /* if this protocol is supported, we can make use of it */
    if (supports_protocol(window, ATOM(WM_TAKE_FOCUS))) {
        return true;
    }

    /* if the client explicitly says it can (or can not) receive focus */
    if ((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
        return window->hints.input != 0;
    }

    /* now we enter a weird area where we really can not be sure if this client
     * can handle focus input, we just check for some window modes and otherwise
     * assume it does accept focus
     */

    if (window->state.mode == WINDOW_MODE_DOCK ||
            window->state.mode == WINDOW_MODE_DESKTOP) {
        return false;
    }

    return true;
}

/* Set the window that is in focus to @window. */
void set_focus_window(Window *window)
{
    if (window != NULL) {
        if (!window->state.is_visible) {
            LOG_ERROR("can not focus an invisible window\n");
            window = NULL;
        } else {
            LOG("focusing window %W\n", window);

            if (!is_window_focusable(window)) {
                LOG_ERROR("the window can not be focused\n");
                window = NULL;
            } else if (window == Window_focus) {
                LOG("the window is already focused\n");
            }
        }
    }

    Window_focus = window;
}
