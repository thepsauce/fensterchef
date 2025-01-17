#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "util.h"
#include "xalloc.h"

/* the first window in the linked list */
Window *g_first_window;

/* list of frames */
struct frame *g_frames;
Frame g_frame_count;
/* the currently selected/focused frame */
Frame g_cur_frame;

/* create a window struct and add it to the window list,
 * this also assigns the next id */
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

    g_values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_dpy, xcb_window,
        XCB_CW_EVENT_MASK, g_values);

    LOG(stderr, "created new window %p\n", (void*) next);
    return next;
}

/* get the frame this window is contained in, may return NULL */
Frame get_frame_of_window(Window *window)
{
    for (Frame frame = 0; frame < g_frame_count; frame++) {
        if (g_frames[frame].window == window){ 
            return frame;
        }
    }
    return (Frame) -1;
}

/* shows the window by mapping and sizing it */
void show_window(Window *window)
{
    Frame frame;

    if (window->visible) {
        LOG(stderr, "tried to show %p but it is already visible\n", (void*) window);
        return;
    }

    frame = get_frame_of_window(window);

    xcb_map_window(g_dpy, window->xcb_window);

    g_values[0] = g_frames[frame].x;
    g_values[1] = g_frames[frame].y;
    g_values[2] = g_frames[frame].w;
    g_values[3] = g_frames[frame].h;
    g_values[4] = 0;
    xcb_configure_window(g_dpy, window->xcb_window,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
        XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);

    window->visible = 1;

    LOG(stderr, "showing window %p\n", (void*) window);
}

/* hides the window by unmapping it */
void hide_window(Window *window)
{
    xcb_unmap_window(g_dpy, window->xcb_window);

    window->visible = 0;

    LOG(stderr, "hiding window %p\n", (void*) window);
}

/* gets the currently focused window */
Window *get_focus_window(void)
{
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        if (w->focused) {
            return w;
        }
    }
    return NULL;
}

/* set the window that is in focus */
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

    LOG(stderr, "focus change from %p to %p\n", (void*) old_focus, (void*) window);
}

/* create a frame at given coordinates that contains a window (`win` may be 0)
 * and attach it to the linked list */
Frame create_frame(Window *window, int32_t x, int32_t y, int32_t w, int32_t h)
{
    struct frame    *next;
    Frame           frame;

    RESIZE(g_frames, g_frame_count + 1);

    frame = g_frame_count++;

    next = &g_frames[frame];
    next->window = window;
    next->x = x;
    next->y = y;
    next->w = w;
    next->h = h;

    LOG(stderr, "frame %" PRId32 " created at %" PRId32 ",%" PRId32 ":%" PRId32 "x%" PRId32,
            frame, x, y, w, h);
    return frame;
}

/* remove a frame from the screen and hide the inner window, this
 * returns 1 when the given frame is the last frame */
int remove_frame(Frame frame)
{
    if (g_frame_count == 0) {
        LOG(stderr, "attempted to remove the only frame %" PRId32 "\n", frame);
        return 1;
    }

    g_frames[frame].window = WINDOW_SENTINEL;

    hide_window(g_frames[frame].window);

    LOG(stderr, "frame %" PRId32 "was removed\n", frame);
    return 0;
}

/* Get a frame at given position. */
Frame get_frame_at_position(int32_t x, int32_t y)
{
    for (Frame frame = 0; frame < g_frame_count; frame++) {
        if (x >= g_frames[frame].x && y >= g_frames[frame].y &&
                x < g_frames[frame].x + g_frames[frame].w &&
                y < g_frames[frame].y + g_frames[frame].h) {
            return frame;
        }
    }
    return (Frame) -1;
}

/* set the frame in focus, this also focuses the inner window if it exists */
void set_focus_frame(Frame frame)
{
    if (g_frames[frame].window != NULL) {
        set_focus_window(g_frames[frame].window);
    }
    g_cur_frame = frame;

    LOG(stderr, "frame %" PRId32 "was focused\n", frame);
}

/* assign a window to an frame without window */
Frame assign_window_to_empty_frame(Frame frame, Window *window) {
    
    g_frames[frame].window = window;

    xcb_map_window(g_dpy, window->xcb_window);

    g_values[0] = g_frames[frame].x;
    g_values[1] = g_frames[frame].y;
    g_values[2] = g_frames[frame].w;
    g_values[3] = g_frames[frame].h;
    g_values[4] = 0;
    xcb_configure_window(g_dpy, window->xcb_window,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
        XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);

    window->visible = 1;
    
    return frame;
}

void reload_frame(Frame frame) {
    if (g_frames[frame].window != NULL && g_frames[frame].window->visible) {
        g_values[0] = g_frames[frame].x;
        g_values[1] = g_frames[frame].y;
        g_values[2] = g_frames[frame].w;
        g_values[3] = g_frames[frame].h;
        g_values[4] = 0;
        xcb_configure_window(g_dpy, g_frames[frame].window->xcb_window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);
    }
}
