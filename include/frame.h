#ifndef FRAME_H
#define FRAME_H

#include <xcb/xcb.h>

#include <stdbool.h>

#include "bits/frame_typedef.h"
#include "bits/window_typedef.h"

typedef enum {
    FRAME_EDGE_LEFT,
    FRAME_EDGE_TOP,
    FRAME_EDGE_RIGHT,
    FRAME_EDGE_BOTTOM,
} frame_edge_t;

typedef enum {
    /* the frame was split horizontally (children are left and right) */
    FRAME_SPLIT_HORIZONTALLY,
    /* the frame was split vertically (children are up and down) */
    FRAME_SPLIT_VERTICALLY,
} frame_split_direction_t;

/* Frames are used to partition a monitor into multiple rectangular regions.
 *
 * When a frame has one child, it must have a second one, so either BOTH left
 * AND right are NULL or neither are NULL.
 * parent is NULL when the frame is a root frame.
 */
struct frame {
    /* the window inside the frame, may be NULL */
    Window *window;

    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* the direction the frame was split in */
    frame_split_direction_t split_direction;

    /* parent of the frame */
    Frame *parent;
    /* left child and right child of the frame */
    Frame *left;
    Frame *right;
};

/* the currently selected/focused frame */
extern Frame *focus_frame;

/* Check if the given point is within the given frame.
 *
 * @return if the point is inside the frame.
 */
bool is_point_in_frame(const Frame *frame, int32_t x, int32_t y);

/* Get a frame at given position.
 *
 * @return a LEAF frame at given position or NULL when there is none.
 */
Frame *get_frame_at_position(int32_t x, int32_t y);

/* Set the size of a frame, this also resize the inner frames and windows. */
void resize_frame(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Resizes the inner window to fit within the frame. */
void reload_frame(Frame *frame);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame *frame);

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(Frame *frame);

#endif
