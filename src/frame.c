#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "util.h"
#include "log.h"
#include "xalloc.h"

/* the first window in the linked list */
Window          *g_first_window;

/* list of frames */
struct frame    *g_frames;
/* the number of allocated frames */
Frame           g_frame_capacity;
/* the currently selected/focused frame */
Frame           g_cur_frame;

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
    LOG("xcb window %" PRId32 " is not managed\n", xcb_window);
    return NULL;
}

/* Get the frame this window is contained in. */
Frame get_frame_of_window(Window *window)
{
    for (Frame frame = 0; frame < g_frame_capacity; frame++) {
        if (g_frames[frame].window == window){ 
            return frame;
        }
    }
    return (Frame) -1;
}

/* Show the window by mapping and sizing it. */
void show_window(Window *window)
{
    Frame frame;

    frame = get_frame_of_window(window);

    if (window->visible) {
        LOG("tried to show %p but it is already visible\n", (void*) window);
        return;
    }

    xcb_map_window(g_dpy, window->xcb_window);

    if (frame != (Frame) -1) {
        g_values[0] = g_frames[frame].x;
        g_values[1] = g_frames[frame].y;
        g_values[2] = g_frames[frame].width;
        g_values[3] = g_frames[frame].height;
        g_values[4] = 0;
        xcb_configure_window(g_dpy, window->xcb_window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);
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
    } while (prev->visible);

    return prev;
}

/* Check if the given point is within the given frame. */
int is_point_in_frame(Frame frame, int32_t x, int32_t y)
{
    return x >= g_frames[frame].x && y >= g_frames[frame].y &&
        (uint32_t) (x - g_frames[frame].x) < g_frames[frame].width &&
        (uint32_t) (y - g_frames[frame].y) < g_frames[frame].height;
}

/* Get the frame at given position.
 * 
 * This recursively traverses the tree array.
 */
Frame get_frame_at_position(int32_t x, int32_t y)
{
    Frame frame = 0, left, right;

    while (1) {
        left = LEFT_FRAME(frame);
        right = RIGHT_FRAME(frame);
        if (IS_FRAME_VALID(left) && is_point_in_frame(left, x, y)) {
            frame = left;
            continue;
        }
        if (IS_FRAME_VALID(right) && is_point_in_frame(right, x, y)) {
            frame = right;
            continue;
        }
        break;
    }
    return frame == 0 ? (is_point_in_frame(frame, x, y) ? 0 : (Frame) -1) :
        frame;
}

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame)
{
    if (g_frames[frame].window != NULL) {
        set_focus_window(g_frames[frame].window);
    }
    g_cur_frame = frame;

    set_notification(UTF8_TEXT("Current frame"),
            g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
            g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);

    LOG("frame %" PRId32 " was focused\n", frame);
}

/* Repositions the underlying window to fit within the frame. */
void reload_frame(Frame frame)
{
    if (g_frames[frame].window != NULL) {
        g_values[0] = g_frames[frame].x;
        g_values[1] = g_frames[frame].y;
        g_values[2] = g_frames[frame].width;
        g_values[3] = g_frames[frame].height;
        g_values[4] = 0;
        xcb_configure_window(g_dpy, g_frames[frame].window->xcb_window,
                XCB_CONFIG_SIZE, g_values);
    }
}
