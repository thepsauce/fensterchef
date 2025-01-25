#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "util.h"
#include "xalloc.h"

/* list of frames */
struct frame    *g_frames;
/* the number of allocated frames */
Frame           g_frame_capacity;
/* the currently selected/focused frame */
Frame           g_cur_frame;

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

/* Set the size of a frame, this also resize the inner windows.
 *
 * This first checks if the sub frames are in a left and right 
 * orientation (horizontal split) or up and down orientation
 * (vertical split) and then resizes the children appropiately.
 *
 * TODO: keep ratio when resizing?
 */
void resize_frame(Frame frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Frame left, right;

    g_frames[frame].x = x;
    g_frames[frame].y = y;
    g_frames[frame].width = width;
    g_frames[frame].height = height;
    reload_frame(frame);

    left = LEFT_FRAME(frame);
    right = RIGHT_FRAME(frame);

    if (!IS_FRAME_VALID(left)) {
        return;
    }

    if (g_frames[left].x != g_frames[right].x) {
        resize_frame(left, x, y, width / 2, height);
        resize_frame(right, x + g_frames[left].width, y,
                width - g_frames[left].width, height);
    } else {
        resize_frame(left, x, y, width, height / 2);
        resize_frame(right, x, y + g_frames[left].height, width,
                height - g_frames[left].height);
    }
}

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame)
{
    if (g_frames[frame].window == NULL ||
            set_focus_window(g_frames[frame].window) != 0) {
        /* setting focus to the root essentially removes focus from all other
         * windows
         */
        xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                SCREEN(g_screen_no)->root, XCB_CURRENT_TIME);
    }
    g_cur_frame = frame;

    set_notification(UTF8_TEXT("Current frame"),
            g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
            g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);

    LOG("frame %" PRIu32 " was focused\n", frame);
}

/* Repositions the underlying window to fit within the frame. */
void reload_frame(Frame frame)
{
    if (g_frames[frame].window != NULL) {
        g_values[0] = g_frames[frame].x;
        g_values[1] = g_frames[frame].y;
        g_values[2] = g_frames[frame].width;
        g_values[3] = g_frames[frame].height;
        g_values[4] = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(g_dpy, g_frames[frame].window->xcb_window,
                XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);
    }
}
