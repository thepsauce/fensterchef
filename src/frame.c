#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "xalloc.h"

/* the first window in the linked list */
Window *g_first_window;

/* the first frame in the linked list */
Frame *g_first_frame;
/* the currently selected/focused frame */
Frame *g_cur_frame;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window)
{
    Window      *next;
    Window      *last;

    for (next = g_first_window; next != NULL; next = next->next) {
        if (next->xcb_window == xcb_window) {
            return next;
        }
    }

    next = xmalloc(sizeof(*next));
    next->xcb_window = xcb_window;
    next->visible = 0;
    next->focused = 0;
    next->next = NULL;

    if (g_first_window == NULL) {
        g_first_window = next;
        next->number = FIRST_WINDOW_NUMBER;
    } else {
        for (last = g_first_window; last->next != NULL; last = last->next) {
            if (last->number + 1 != last->next->number) {
                break;
            }
        }
        next->number = last->number + 1;

        while (last->next != NULL) {
            last = last->next;
        }
        last->next = next;
    }

    g_values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_dpy, xcb_window,
        XCB_CW_EVENT_MASK, g_values);

    LOG("created new window %p\n", (void*) next);
    return next;
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

    LOG("destroyed window %p\n", (void*) window);

    free(window);
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
    LOG("xcb window %" PRId32 " is not managed\n", xcb_window);
    return NULL;
}

/* Get the frame this window is contained in. */
Frame *get_frame_of_window(Window *window)
{
    for (Frame *frame = g_first_frame; frame != NULL; frame = frame->next) {
        if (frame->window == window){ 
            return frame;
        }
    }
    return NULL;
}

/* Show the window by mapping and sizing it. */
void show_window(Window *window)
{
    Frame *frame;

    if (window->visible) {
        LOG("tried to show %p but it is already visible\n", (void*) window);
        return;
    }

    xcb_map_window(g_dpy, window->xcb_window);

    frame = get_frame_of_window(window);
    if (frame != NULL) {
        g_values[0] = frame->x;
        g_values[1] = frame->y;
        g_values[2] = frame->w;
        g_values[3] = frame->h;
        g_values[4] = 0;
        xcb_configure_window(g_dpy, window->xcb_window,
                XCB_CONFIG_SIZE, g_values);
    }

    window->visible = 1;

    LOG("showing window %p\n", (void*) window);
}

/* Hide the window by unmapping it. */
void hide_window(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);

    window->visible = 0;

    LOG("hiding window %p\n", (void*) window);
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
    if (old_focus == window) {
        return;
    }
    if (old_focus != NULL) {
        old_focus->focused = 0;
    }
    window->focused = 1;

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, window->xcb_window,
            XCB_CURRENT_TIME);

    LOG("focus change from %p to %p\n", (void*) old_focus, (void*) window);
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
    } while (next->visible);

    return next;
}

/* Get a window that is not shown but in the window list coming before
 * the given window. */
Window *get_prev_hidden_window(Window *window)
{
    Window *prev, *prev_prev;

    if (window == NULL) {
        return NULL;
    }
    prev = window;
    do {
        for (prev_prev = g_first_window; prev_prev->next != prev; ) {
            if (prev_prev->next == NULL) {
                break;
            }
            prev_prev = prev_prev->next;
        }
        prev = prev_prev;
        if (window == prev) {
            return NULL;
        }
    } while (prev->visible);

    return prev;
}

/* Create a frame at given coordinates that contains a window
 * (`window` may be NULL) and attach it to the linked list. */
Frame *create_frame(Window *window, int32_t x, int32_t y, int32_t w, int32_t h)
{
    Frame *next;
    Frame *last;

    next = xmalloc(sizeof(*next));
    next->window = window;
    next->x = x;
    next->y = y;
    next->w = w;
    next->h = h;
    next->next = NULL;

    if (g_first_frame == NULL) {
        g_cur_frame = next;
        g_first_frame = next;
    } else {
        for (last = g_first_frame; last->next != NULL; ) {
            last = last->next;
        }
        last->next = next;
    }

    LOG("frame %p created at %" PRId32 ",%" PRId32 ":%" PRId32 "x%" PRId32,
            (void*) next, x, y, w, h);
    return next;
}

/* Remove a frame from the screen and hide the inner window. */
int remove_frame(Frame *frame)
{
    Frame *prev;

    if (g_first_frame->next == NULL) {
        LOG("attempted to remove the only frame %p\n", (void*) frame);
        return 1;
    }

    if (frame == g_first_frame) {
        g_first_frame = frame->next;
    } else {
        for (prev = g_first_frame; prev->next != frame; ) {
            prev = prev->next;
        }
        prev->next = frame->next;
    }

    hide_window(frame->window);

    LOG("frame %p was removed\n", (void*) frame);

    free(frame);
    return 0;
}

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame *frame)
{
    if (frame->window != NULL) {
        set_focus_window(frame->window);
    }
    g_cur_frame = frame;

    LOG("frame %p was focused\n", (void*) frame);
}
