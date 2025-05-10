#include <inttypes.h>
#include <limits.h>
#include <string.h>

#include "binding.h"
#include "event.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "parse/parse.h"
#include "parse/stream.h"
#include "window.h"
#include "window_properties.h"
#include "window_stacking.h"
#include "x11_synchronize.h"

/* the window associations */
struct window_association *window_associations;

/* the number of elments in `window_associations` */
unsigned number_of_window_associations;

/* the number of all windows within the linked list, this value is kept up to
 * date through `create_window()` and `destroy_window()`
 */
unsigned Window_count;

/* the window that was created before any other */
FcWindow *Window_oldest;

/* the window at the bottom of the Z stack */
FcWindow *Window_bottom;

/* the window at the top of the Z stack */
FcWindow *Window_top;

/* the window at the top of the Z stack on the server */
FcWindow *Window_server_top;

/* the first window in the number linked list */
FcWindow *Window_first;

/* the currently focused window */
FcWindow *Window_focus;

/* the last pressed window, this only gets set when a window is pressed by a
 * grabbed button
 */
FcWindow *Window_pressed;

/* Increment the reference count of the window. */
inline void reference_window(FcWindow *window)
{
    window->reference_count++;
}

/* Decrement the reference count of the window and free @window when it reaches
 * 0.
 */
inline void dereference_window(FcWindow *window)
{
    window->reference_count--;
    if (window->reference_count == 0) {
        free(window);
    }
}

/* Add a new association from window instance/class to actions. */
void add_window_associations(struct window_association *associations,
        unsigned number_of_associations)
{
    /* prepend the associations */
    REALLOCATE(window_associations,
            number_of_associations + number_of_window_associations);
    /* make a gap at the front */
    MOVE(&window_associations[number_of_associations],
            &window_associations[0],
            number_of_window_associations);
    /* move them into the front */
    COPY(&window_associations[0], associations, number_of_associations);
    number_of_window_associations += number_of_associations;
}

/* Clear all currently set window associations. */
void clear_window_associations(void)
{
    for (unsigned i = 0; i < number_of_window_associations; i++) {
        struct window_association *const association = &window_associations[i];
        free(association->instance_pattern);
        free(association->class_pattern);
        clear_action_list(&association->actions);
    }
    free(window_associations);

    window_associations = NULL;
    number_of_window_associations = 0;
}

/* Run the action associated to given window. */
bool run_window_association(FcWindow *window)
{
    if (window->instance == NULL && window->class == NULL) {
        return false;
    }

    /* if the class property is set, try to find an association */
    for (unsigned i = 0; i < number_of_window_associations; i++) {
        struct window_association *const association = &window_associations[i];
        if ((association->instance_pattern == NULL ||
                    matches_pattern(association->instance_pattern,
                        window->instance)) &&
                matches_pattern(association->class_pattern, window->class)) {
            LOG_DEBUG("running associated actions: %A\n",
                    &association->actions);

            FcWindow *const old_pressed = Window_pressed;
            Window_pressed = window;
            run_action_list(&association->actions);
            Window_pressed = old_pressed;
            return true;
        }
    }

    return false;
}

/* Find where in the number linked list a gap is.
 *
 * @return NULL when the window should be inserted before the first window.
 */
static inline FcWindow *find_number_gap(void)
{
    FcWindow *previous;

    /* if the first window has window number greater than the first number that
     * means there is space at the front
     */
    if (Window_first->number > configuration.first_window_number) {
        return NULL;
    }

    previous = Window_first;
    /* find the first window with a higher number than
     * `first_window_number`
     */
    for (; previous->next != NULL; previous = previous->next) {
        if (previous->next->number > configuration.first_window_number) {
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
static inline FcWindow *find_window_number(unsigned number)
{
    FcWindow *previous;

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
FcWindow *create_window(Window id)
{
    XWindowAttributes attributes;
    Window root;
    int x, y;
    unsigned int width, height;
    unsigned int border_width;
    unsigned int depth;
    XSetWindowAttributes set_attributes;
    FcWindow *window;
    FcWindow *previous;
    window_mode_t mode;

    XGetWindowAttributes(display, id, &attributes);

    /* Override redirect is used by windows to indicate that our window manager
     * should not tamper with them.  We also check if the class is InputOnly
     * which is not a case we want to handle (the window has no graphics).
     */
    if (attributes.override_redirect || attributes.class == InputOnly) {
        /* check if the window holds a command */
        utf8_t *const command = get_fensterchef_command_property(id);
        if (command != NULL) {
            LOG("window %#lx has command: %s\n",
                    id, command);

            (void) initialize_string_stream(command);
            (void) parse_stream_and_run_actions();

            free(command);

            /* signal to the window that we executed its command and it can go
             * now
             */
            XDeleteProperty(display, id, ATOM(FENSTERCHEF_COMMAND));
        }
        return NULL;
    }

    /* set the initial border color */
    set_attributes.border_pixel = configuration.border_color;
    /* we want to know if if any properties change */
    set_attributes.event_mask = PropertyChangeMask;
    XChangeWindowAttributes(display, id, CWBorderPixel | CWEventMask,
            &set_attributes);

    XGetGeometry(display, id, &root, &x, &y, &width, &height, &border_width,
            &depth);

    ALLOCATE_ZERO(window, 1);

    window->reference_count = 1;
    window->client.id = id;
    window->client.x = x;
    window->client.y = x;
    window->client.width = width;
    window->client.height = height;
    window->client.border = configuration.border_color;
    /* check if the window is already mapped on the X server */
    if (attributes.map_state != IsUnmapped) {
        window->client.is_mapped = true;
    }

    /* start off with an invalid mode, this gets set below */
    window->state.mode = WINDOW_MODE_MAX;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->border_color = window->client.border;

    /* setup window properties and get the initial mode */
    initialize_window_properties(window, &mode);

    /* link into the Z, age and number linked lists */
    if (Window_first == NULL) {
        Window_oldest = window;
        Window_bottom = window;
        Window_top = window;
        Window_server_top = window;
        Window_first = window;
        if (window->number == 0) {
            window->number = configuration.first_window_number;
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
                window->number = configuration.first_window_number;
            }
        } else {
            if (window->number == 0) {
                if (previous->number < configuration.first_window_number) {
                    window->number = configuration.first_window_number;
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

        /* Put the window at the top of the Z server linked list.  That is where
         * the server puts new windows.
         */
        window->server_below = Window_server_top;
        Window_server_top->server_above = window;
        Window_server_top = window;

        /* TODO: do we need to make sure the window is REALLY at the top? */

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

    /* grab the buttons for this window */
    grab_configured_buttons(id);

    LOG("created new window %W\n",
            window);

    if (run_window_association(window)) {
        /* nothing */
    /* if a window does not start in normal state, do not map it */
    } else if ((window->hints.flags & StateHint) &&
            window->hints.initial_state != NormalState) {
        LOG("window %W starts off as hidden window\n",
                window);
    } else {
        /* run the default behavior */
        show_window(window);
        if (is_window_focusable(window)) {
            set_focus_window_with_frame(window);
        }
    }

    return window;
}

/* Destroys given window and removes it from the window linked list. */
void destroy_window(FcWindow *window)
{
    Frame *frame;
    FcWindow *previous;

    /* Really make sure the window is hidden.  Not sure if this case can ever
     * happen because usually a MapUnnotify event hides the window beforehand.
     */
    hide_window_abruptly(window);

    /* exceptional state, this should never happen */
    if (window == Window_focus) {
        Window_focus = NULL;
        LOG_ERROR("destroying window with focus\n");
    }

    if (window == Window_pressed) {
        Window_pressed = NULL;
    }

    /* this should also never happen but we check just in case */
    frame = get_window_frame(window);
    if (frame != NULL) {
        frame->window = NULL;
        LOG_ERROR("window being destroyed is still within a frame\n");
    }

    LOG("destroying window %W\n", window);

    /* remove from the z linked list */
    unlink_window_from_z_list(window);

    /* remove from the server z linked list */
    unlink_window_from_z_server_list(window);

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

    /* setting the id to None marks the window as destroyed */
    window->client.id = None;
    free(window->name);
    free(window->protocols);
    free(window->states);

    dereference_window(window);
}

/* Get a window with given @number or NULL if no window has that id. */
FcWindow *get_window_by_number(unsigned number)
{
    FcWindow *window;

    for (window = Window_first; window != NULL; window = window->next) {
        if (window->number == number) {
            break;
        }
    }
    return window;
}

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(FcWindow *window)
{
    time_t current_time;
    XEvent event;

    current_time = time(NULL);
    /* if either `WM_DELETE_WINDOW` is not supported or a close was requested
     * twice in a row
     */
    if (!supports_protocol(window, ATOM(WM_DELETE_WINDOW)) ||
            (window->state.was_close_requested && current_time <=
                window->state.user_request_close_time +
                    REQUEST_CLOSE_MAX_DURATION)) {
        XKillClient(display, window->client.id);
        return;
    }

    memset(&event, 0, sizeof(event));

    /* bake an event for running a protocol on the window */
    event.type = ClientMessage;
    event.xclient.window = window->client.id;
    event.xclient.message_type = ATOM(WM_PROTOCOLS);
    event.xclient.format = 32;
    event.xclient.data.l[0] = ATOM(WM_DELETE_WINDOW);
    XSendEvent(display, window->client.id, False, NoEventMask, &event);

    window->state.was_close_requested = true;
    window->state.user_request_close_time = current_time;
}

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int *x, int *y,
        unsigned int width, unsigned int height, int gravity)
{
    switch (gravity) {
    /* attach to the top left */
    case NorthWestGravity:
        *x = monitor->x;
        *y = monitor->y;
        break;

    /* attach to the top */
    case NorthGravity:
        *y = monitor->y;
        break;

    /* attach to the top right */
    case NorthEastGravity:
        *x = monitor->x + monitor->width - width;
        *y = monitor->y;
        break;

    /* attach to the left */
    case WestGravity:
        *x = monitor->x;
        break;

    /* put it into the center */
    case CenterGravity:
        *x = monitor->x + (monitor->width - width) / 2;
        *y = monitor->y + (monitor->height - height) / 2;
        break;

    /* attach to the right */
    case EastGravity:
        *x = monitor->x + monitor->width - width;
        break;

    /* attach to the bottom left */
    case SouthWestGravity:
        *x = monitor->x;
        *y = monitor->y + monitor->height - height;
        break;

    /* attach to the bottom */
    case SouthGravity:
        *y = monitor->y + monitor->height - height;
        break;

    /* attach to the bottom right */
    case SouthEastGravity:
        *x = monitor->x + monitor->width - width;
        *y = monitor->y + monitor->width - height;
        break;

    /* nothing to do */
    default:
        break;
    }
}

/* Get the minimum size the window can have. */
void get_minimum_window_size(const FcWindow *window, Size *size)
{
    unsigned int width = 0, height = 0;

    if (window->state.mode != WINDOW_MODE_TILING) {
        if ((window->size_hints.flags & PMinSize)) {
            width = window->size_hints.min_width;
            height = window->size_hints.min_height;
        }
    }
    size->width = MAX(width, WINDOW_MINIMUM_SIZE);
    size->height = MAX(height, WINDOW_MINIMUM_SIZE);
}

/* Get the maximum size the window can have. */
void get_maximum_window_size(const FcWindow *window, Size *size)
{
    unsigned int width = UINT_MAX, height = UINT_MAX;

    if ((window->size_hints.flags & PMaxSize)) {
        width = window->size_hints.max_width;
        height = window->size_hints.max_height;
    }
    size->width = MIN(width, WINDOW_MAXIMUM_SIZE);
    size->height = MIN(height, WINDOW_MAXIMUM_SIZE);
}

/* Set the position and size of a window. */
void set_window_size(FcWindow *window, int x, int y, unsigned int width,
        unsigned int height)
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
}

/* Get the internal window that has the associated X window. */
FcWindow *get_fensterchef_window(Window id)
{
    for (FcWindow *window = Window_first; window != NULL;
            window = window->next) {
        if (window->client.id == id) {
            return window;
        }
    }
    return NULL;
}

/* Checks if @frame contains @window and checks this for all its children. */
static Frame *find_frame_recursively(Frame *frame, const FcWindow *window)
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
Frame *get_window_frame(const FcWindow *window)
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
bool is_window_focusable(FcWindow *window)
{
    /* if this protocol is supported, we can make use of it */
    if (supports_protocol(window, ATOM(WM_TAKE_FOCUS))) {
        return true;
    }

    /* if the client explicitly says it can (or can not) receive focus */
    if ((window->hints.flags & InputHint)) {
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
void set_focus_window(FcWindow *window)
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

/* Focus @window and the frame it is contained in if any. */
void set_focus_window_with_frame(FcWindow *window)
{
    Frame *frame;

    set_focus_window(window);

    frame = get_window_frame(window);
    if (frame != NULL) {
        Frame_focus = frame;
    }
}
