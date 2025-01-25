#ifndef FRAME_H
#define FRAME_H

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h> // xcb_size_hints_t

/* an invalid frame index */
#define FRAME_SENTINEL ((Frame) -1)

/* Get the parent frame of a child frame. */
#define PARENT_FRAME(frame) (((frame)-1)/2)

/* Get the left child of a parent frame. */
#define LEFT_FRAME(frame) ((frame)*2+1)

/* Get the right child of a parent frame. */
#define RIGHT_FRAME(frame) ((frame)*2+2)

/* Check if a given frame is valid. */
#define IS_FRAME_VALID(frame) \
    ((frame) < g_frame_capacity && \
      g_frames[frame].window != WINDOW_SENTINEL)

/* forward declaration */
struct window;

/* A frame is a rectangular region on the screen that may or may not
 * contain a window.
 *
 * Frames can be split into smaller sub frames and are the driving
 * force of the tiling layout.
 */
struct frame {
    /* the window inside the frame, may be NULL or
     * WINDOW_SENTINEL to signal an unused frame */
    struct window *window;
    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

/* a frame is accessed by an index in the frames array */
typedef uint32_t Frame;

/* This is how frames are organize in the array g_frames:
 *     0          1              2
 * [ Frame, Frame * 2 + 1, Frame * 2 + 2, ... ]
 *           Left child     Right child
 *
 *   0  1    2    3  4        5              6
 * [ X, X, Frame, X, X, Frame * 2 + 1, Frame * 2 + 2, ... ]
 *                       Left child     Right child
 *
 * NOTE: The array has gaps and such gaps are indicated with their window
 * being set to WINDOW_SENTINEL.
 *
 * It is guaranteed that there is always at least one frame (the root frame).
 */

/* list of frames */
extern struct frame *g_frames;
/* the number of allocated frames in g_frames */
extern Frame        g_frame_capacity;

/* the currently selected/focused frame */
extern Frame        g_cur_frame;

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame);

/* Check if the given point is within the given frame.
 *
 * @return 1 if the point is inside the frame, 0 otherwise */
int is_point_in_frame(Frame frame, int32_t x, int32_t y);

/* Get a frame at given position. */
Frame get_frame_at_position(int32_t x, int32_t y);

/* Set the size of a frame, this also resize the inner frames and windows. */
void resize_frame(Frame frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame);

/* Reposition the underlying window to fit within the frame.
 *
 * If the underlying window is NULL, nothing happens.
 */
void reload_frame(Frame frame);

#endif
