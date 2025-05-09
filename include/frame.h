#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>

#include "bits/frame.h"
#include "bits/window.h"
#include "utility/types.h"

/* the minimum width or height of a frame, frames are never clipped to this size
 * and can even have a size of 0, it is used when resizing frames
 */
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
 * That is why `frame->left != NULL` is always used as a test for checking if a
 * frame has children or not.
 *
 * `parent` is NULL when the frame is a root frame or stashed frame.
 */
struct frame {
    /* reference counter to keep the pointer around for longer after the frame
     * has been destroyed
     */
    unsigned reference_count;

    /* the window inside the frame, may be NULL; this might become a destroyed
     * window when this frame is stashed, to check this use `window->client.id`,
     * it should be `None` when the window is destroyed
     */
    FcWindow *window;

    /* coordinates and size of the frame */
    int x;
    int y;
    unsigned width;
    unsigned height;

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
    unsigned number;
};

/* the last frame in the frame stashed linked list */
extern Frame *Frame_last_stashed;

/* the currently selected/focused frame */
extern Frame *Frame_focus;

/* Increment the reference count of the frame. */
void reference_frame(Frame *frame);

/* Decrement the reference count of the frame and free @frame when it reaches 0.
 */
void dereference_frame(Frame *frame);

/* Create a frame object. */
Frame *create_frame(void);

/* Free the frame object.
 *
 * @frame must have no parent or children and it shall not be the root frame
 *        of a monitor.
 */
void destroy_frame(Frame *frame);

/* Look through all visible frames to find a frame with given @number. */
Frame *get_frame_by_number(unsigned number);

/* Check if the given @frame has no splits and no window. */
bool is_frame_void(const Frame *frame);

/* Check if the given point is within the given frame.
 *
 * @return if the point is inside the frame.
 */
bool is_point_in_frame(const Frame *frame, int x, int y);

/* Get a frame at given position.
 *
 * @return a LEAF frame at given position or NULL when there is none.
 */
Frame *get_frame_at_position(int x, int y);

/* Replace @frame with @with.
 *
 * @frame receives the children or the window within @with and the
 *        number, split direction and ration @with has.
 *        @frame only keeps its size.
 *        @frame should be a void (pass `is_frame_void()`).
 * @with is emptied by this function, only the original size remains.
 */
void replace_frame(Frame *frame, Frame *with);

/* Get the gaps the frame applies to its inner window. */
void get_frame_gaps(const Frame *frame, Extents *gaps);

/* Resizes the inner window to fit within the frame. */
void reload_frame(Frame *frame);

/* Set the frame in focus, this also focuses an associated window if possible.
 *
 * The associated window is either a window covering the monitor the frame is on
 * or the window within the frame.
 *
 * If you just want to set the focused frame without focusing the inner window:
 * `Frame_focus = frame` suffices.
 */
void set_focus_frame(Frame *frame);

/* Get the frame above the given one that has no parent.
 *
 * @frame may be NULL, then simply NULL is returned.
 */
Frame *get_root_frame(const Frame *frame);

#endif
