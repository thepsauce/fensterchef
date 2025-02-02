#ifndef FRAME_H
#define FRAME_H

#include <xcb/xcb.h>

/* Frames are used to partition a monitor into multiple rectangular regions.
 *
 * When a frame has one child, it must have a second one, so either BOTH left
 * AND right are NULL or neither are NULL.
 * parent is NULL when the frame is a root frame.
 */
typedef struct frame {
    /* the window inside the frame, may be NULL */
    struct window *window;

    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* parent of the frame */
    struct frame *parent;
    /* left and right child of the frame */
    struct frame *left;
    struct frame *right;
} Frame;

/* the currently selected/focused frame */
extern Frame *g_cur_frame;

/* Check if the given point is within the given frame.
 *
 * @return 1 if the point is inside the frame, 0 otherwise.
 */
int is_point_in_frame(const Frame *frame, int32_t x, int32_t y);

/* Get a frame at given position.
 *
 * @return a LEAF frame at given position or NULL when there is none.
 */
Frame *get_frame_at_position(int32_t x, int32_t y);

/* Set the size of a frame, this also resize the inner frames and windows. */
void resize_frame(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame *frame);

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(Frame *frame);

#endif
