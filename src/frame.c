#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "screen.h"
#include "util.h"
#include "window.h"
#include "xalloc.h"

/* the currently selected/focused frame */
Frame *focus_frame;

/* Check if the given point is within the given frame. */
int is_point_in_frame(const Frame *frame, int32_t x, int32_t y)
{
    return x >= frame->x && y >= frame->y &&
        (uint32_t) (x - frame->x) < frame->width &&
        (uint32_t) (y - frame->y) < frame->height;
}

/* Get the frame at given position. */
Frame *get_frame_at_position(int32_t x, int32_t y)
{
    Frame *frame;

    for (Monitor *monitor = screen->monitor; monitor != NULL;
            monitor = monitor->next) {
        frame = monitor->frame;
        if (is_point_in_frame(frame, x, y)) {
            /* recursively move into child frame until we are at a leaf */
            while (frame->left != NULL) {
                if (is_point_in_frame(frame->left, x, y)) {
                    frame = frame->left;
                    continue;
                }
                if (is_point_in_frame(frame->right, x, y)) {
                    frame = frame->right;
                    continue;
                }
            }
            return frame;
        }
    }
    return NULL;
}

/* Set the size of a frame, this also resize the inner frames and windows.
 *
 * TODO: keep ratio when resizing?
 */
void resize_frame(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Frame *left, *right;

    frame->x = x;
    frame->y = y;
    frame->width = width;
    frame->height = height;
    if (frame->window != NULL) {
        set_window_size(frame->window, x, y, width, height);
    }

    left = frame->left;
    right = frame->right;

    /* check if the frame has children */
    if (left == NULL) {
        return;
    }

    /* check if the split is horizontal or vertical and place the children at
     * half the area
     */
    if (left->x != right->x) {
        resize_frame(left, x, y, width / 2, height);
        resize_frame(right, x + left->width, y, width - left->width, height);
    } else {
        resize_frame(left, x, y, width, height / 2);
        resize_frame(right, x, y + left->height, width, height - left->height);
    }
}

/* Set the frame in focus, this also focuses the inner window if possible. */
void set_focus_frame(Frame *frame)
{
    if (frame->window != NULL) {
        set_focus_window(frame->window);
    }
    focus_frame = frame;

    set_notification(UTF8_TEXT("Current frame"),
            focus_frame->x + focus_frame->width / 2,
            focus_frame->y + focus_frame->height / 2);

    LOG("frame %p was focused\n", (void*) frame);
}

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(Frame *frame)
{
    while (frame->parent != NULL) {
        frame = frame->parent;
    }
    return frame;
}
