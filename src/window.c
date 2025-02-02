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

    update_all_window_properties(window);

    set_window_state(window, predict_window_state(window), 0);

    LOG("created new window: %s\n", window->properties.short_name);
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
    LOG("setting window %" PRIu32 " above all other windows\n", window->number);

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

    LOG("trying to change focus to %" PRIu32, window->number);

    if ((window->properties.hints.flags & XCB_ICCCM_WM_HINT_INPUT) &&
            window->properties.hints.input == 0) {
        LOG_ADDITIONAL(", but the window does not accept focus\n");
        return 1;
    }

    old_focus = get_focus_window();
    if (old_focus != NULL) {
        old_focus->focused = 0;
    }

    window->focused = 1;

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
    window->focused = 0;
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
    } while (other->state.current != WINDOW_STATE_SHOWN &&
            other->state.current != WINDOW_STATE_POPUP);

    if (other->frame != NULL) {
        set_focus_frame(other->frame);
    } else {
        other->focused = 1;
        xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                window->xcb_window, XCB_CURRENT_TIME);
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
