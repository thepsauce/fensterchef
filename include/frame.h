#ifndef FRAME_H
#define FRAME_H

#include <xcb/xcb.h>

#include <stdbool.h>

#include "bits/frame_typedef.h"
#include "bits/window_typedef.h"

#include "utility.h"

/* the minimum width or height of a frame */
#define FRAME_MINIMUM_SIZE 12

/* an edge of the frame */
typedef enum {
    /* the left edge */
    FRAME_EDGE_LEFT,
    /* the top edge */
    FRAME_EDGE_TOP,
    /* the right edge */
    FRAME_EDGE_RIGHT,
    /* the bottom edge */
    FRAME_EDGE_BOTTOM,
} frame_edge_t;

/* a direction to split a frame in */
typedef enum {
    /* the frame was split horizontally (children are left and right) */
    FRAME_SPLIT_HORIZONTALLY,
    /* the frame was split vertically (children are up and down) */
    FRAME_SPLIT_VERTICALLY,
} frame_split_direction_t;

/* Frames are used to partition a monitor into multiple rectangular regions.
 *
 * When a frame has one child, it must have a second one, so either BOTH left
 * AND right are NULL OR neither are NULL.
 *
 * `parent` is NULL when the frame is a root frame or stashed frame.
 */
struct frame {
    /* the window inside the frame, may be NULL; it can also happen that this
     * becomes a completely invalid memory address but only if the frame is
     * stashed, these invalid windows are resolved once the frame becomes
     * unstashed
     */
    Window *window;

    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* ration between the two children */
    Ratio ratio;

    /* the direction the frame was split in */
    frame_split_direction_t split_direction;

    /* if a parent frame is focused, this parent stores from which child it
     * was focused from
     */
    bool moved_from_left;

    /* parent of the frame */
    Frame *parent;
    /* left child and right child of the frame */
    Frame *left;
    Frame *right;

    /* the previous stashed frame in the frame stashed linked list */
    Frame *previous_stashed;

    /* the id of this frame, this is a unique number, the exception is 0 */
    uint32_t number;
};

/* the last frame in the frame stashed linked list */
extern Frame *Frame_last_stashed;

/* the currently selected/focused frame */
extern Frame *Frame_focus;

/* the focus that existed before entering the event loop */
extern Frame *Frame_old_focus;

/* Create a frame object. */
Frame *create_frame(void);

/* Frees the frame object (but not the child frames). */
void destroy_frame(Frame *frame);

/* Look through all visible frames to find a frame with given @number. */
Frame *get_frame_by_number(uint32_t number);

/* Check if the given @frame has no splits and no window. */
bool is_frame_void(const Frame *frame);

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

/* Set the size of a frame, this also resize the child frames and windows. */
void resize_frame(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Set the size of a frame, this also resizes the child frames and windows.
 *
 * This function ignores the @frame->ratio and instead uses the existing ratio
 * between the windows to size them.
 *
 * This function is good for reloading child frames if the parent resized.
 */
void resize_frame_and_ignore_ratio(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Replace @frame (windows and child frames) with @with.
 *
 * @with is emptied by this function, only the original size remains.
 */
void replace_frame(Frame *frame, Frame *with);

/* Get the gaps the frame applies to its inner window. */
void get_frame_gaps(const Frame *frame, Extents *gaps);

/* Resizes the inner window to fit within the frame. */
void reload_frame(Frame *frame);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame *frame);

/* Focus @window and the frame it is in. */
void set_focus_window_with_frame(Window *window);

/* Get the frame above the given one that has no parent.
 *
 * @frame may be NULL, then simply NULL is returned.
 */
Frame *get_root_frame(const Frame *frame);

#endif
