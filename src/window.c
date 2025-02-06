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
Window *g_first_window;

/* the currently focused window */
static Window *focus_window;

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
    window->next = g_first_window;
    g_first_window = window;

    /* preserve the event mask of our windows */
    window_attributes_cookie = xcb_get_window_attributes(g_dpy, xcb_window);
    window_attributes = xcb_get_window_attributes_reply(g_dpy,
            window_attributes_cookie, NULL);
    if (window_attributes == NULL) {
        g_values[0] = 0;
    } else {
        g_values[0] = window_attributes->your_event_mask;
        free(window_attributes);
    }

    g_values[0] |= XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes(g_dpy, xcb_window, XCB_CW_EVENT_MASK,
            g_values);

    g_values[0] = 0;
    xcb_configure_window(g_dpy, xcb_window, XCB_CONFIG_WINDOW_BORDER_WIDTH,
            g_values);

    update_all_window_properties(window);
    synchronize_all_window_properties(window);

    LOG("created new window wrapping %" PRIu32 "\n", xcb_window);
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

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height)
{
    LOG("configuring size of window %" PRIu32 " to: %" PRId32 ", %" PRId32 ", %" PRIu32 ", %" PRIu32 "\n",
            window->number, x, y, width, height);

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
    Window *above;

    LOG("setting window %" PRIu32 " above all other windows\n", window->number);

    /* desktop windows are always at the bottom */
    if (does_window_have_type(window, ewmh._NET_WM_WINDOW_TYPE_DESKTOP)) {
        g_values[0] = XCB_STACK_MODE_BELOW;
    } else {
        g_values[0] = XCB_STACK_MODE_ABOVE;
    }
    xcb_configure_window(g_dpy, window->xcb_window,
            XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    /* put windows that are transient for this window above it */
    for (above = g_first_window; above != NULL; above = above->next) {
        if (!above->state.is_visible) {
            continue;
        }
        if (above->properties.transient_for == window->xcb_window) {
            xcb_configure_window(g_dpy, window->xcb_window,
                    XCB_CONFIG_WINDOW_STACK_MODE, g_values);
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
    return focus_window;
}

/* Set the window that is in focus. */
int set_focus_window(Window *window)
{
    if (window == NULL) {
        LOG("focus change to root\n");
        focus_window = NULL;
        synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);
        return 1;
    }

    LOG("trying to change focus to %" PRIu32, window->number);

    if ((window->properties.hints.flags & XCB_ICCCM_WM_HINT_INPUT) &&
            window->properties.hints.input == 0) {
        LOG_ADDITIONAL(", but the window does not accept focus\n");
        return 1;
    }

    focus_window = window;
    synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);

    LOG_ADDITIONAL(" and succeeded\n");

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, window->xcb_window,
            XCB_CURRENT_TIME);
    return 0;
}

/* Gives any window different from given window focus. */
void give_someone_else_focus(Window *window)
{
    Window *other;

    /* TODO: when the window is a popup window, focus the window below it,
     * when it is a tiling window, JUST GET RID OF THIS FUNCTION
     */
    /* TODO: use a focus stack? */
    other = window;
    do {
        if (other->next == NULL) {
            other = g_first_window;
        } else {
            other = window->next;
        }
        if (other == window) {
            return;
        }
    } while (other->state.is_visible);

    if (other->frame != NULL) {
        set_focus_frame(other->frame);
    } else {
        set_focus_window(other);
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
            next = g_first_window;
        } else {
            next = next->next;
        }
        if (window == next) {
            return NULL;
        }
    } while (!next->state.is_mappable || next->state.is_visible);

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
    } while (!previous->state.is_mappable || previous->state.is_visible);

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
