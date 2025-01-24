#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#include <fontconfig/fontconfig.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

/* an invalid window pointer */
#define WINDOW_SENTINEL ((Window*) -1)

/* all possible window states */
#define WINDOW_STATE_HIDDEN 0
#define WINDOW_STATE_SHOWN 1
#define WINDOW_STATE_POPUP 2
#define WINDOW_STATE_IGNORE 3
#define WINDOW_STATE_FULLSCREEN 4

/* A window is a wrapper around an xcb window, it is always part of a global
 * linked list and has a unique id.
 *
 * A window may be in different states:
 * HIDDEN: The window is not shown but would usually go in the tiling layout.
 * SHOWN: The window is part of the current tiling layout.
 * POPUP: The window is a popup window, it may or may not be visible.
 * IGNORE: The window was once registered by the WM but the user decided to
 *      ignore it. It does not appear when cycling through windows.
 * FULLSCREEN: The window covers the entire screen.
 */
typedef struct window {
    /* the actual X window */
    xcb_window_t xcb_window;
    /* xcb size hints of the window */
    xcb_size_hints_t size_hints;
    /* short window title */
    FcChar8 short_title[256];
    /* saved window when it was in popup state */
    uint32_t popup_width;
    uint32_t popup_height;
    /* the window state, one of WINDOW_STATE_* */
    unsigned state : 3;
    /* the previous window state */
    unsigned prev_state : 3;
    /* if the user forced this window to be a certain state */
    unsigned forced_state : 1;
    /* if the window has focus */
    unsigned focused : 1;
    /* the id of this window */
    uint32_t number;
    /* the next window in the linked list */
    struct window *next;
} Window;

/* the first window in the linked list, the list is sorted increasingly
 * with respect to the window number
 */
extern Window       *g_first_window;

/* Get the parent frame of a child frame. */
#define PARENT_FRAME(frame) (((frame)-1)/2)

/* Get the left child of a parent frame. */
#define LEFT_FRAME(frame) ((frame)*2+1)

/* Get the right child of a parent frame. */
#define RIGHT_FRAME(frame) ((frame)*2+2)

/* Hheck if a given frame index is valid. */
#define IS_FRAME_VALID(frame) \
    ((frame) < g_frame_capacity && \
      g_frames[frame].window != WINDOW_SENTINEL)

/* A frame is a rectangular region on the screen that may or may not
 * contain a window.
 *
 * Frames can be split into smaller sub frames and are the driving
 * force of the tiling layout.
 */
struct frame {
    /* the window inside the frame, may be NULL or
     * WINDOW_SENTINEL to signal an unused frame */
    Window *window;
    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

/* a frame is accessed by an index in the frames array */
typedef uint32_t Frame;

/* This is how frames are organize in the array g_frames:
 *    0         1               2
 * [ Frame, Frame * 2 + 1, Frame * 2 + 2 ]
 *
 * 0  1    2    3  4  5      6           7
 * [ X, X, Frame, ., ., ., Frame * 2, Frame * 2 + 1 ]
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

/* -- Implemented in window.c -- */

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying xcb window.
 */
void destroy_window(Window *window);

/* Get the window before this window in the linked list.
 * This function WRAPS around so
 *  `get_previous_window(g_first_window)` returns the last window.
 *
 * @window may be NULL, then NULL is also returned.
 */
Window *get_previous_window(Window *window);

/* Get the internal window that has the associated xcb window.
 *
 * @return NULL when none has this xcb window.
 */
Window *get_window_of_xcb_window(xcb_window_t xcb_window);

/* Get the frame this window is contained in.
 *
 * @return NULL when the window is not in any frame.
 */
Frame get_frame_of_window(Window *window);

/* Get the currently focused window.
 *
 * @return NULL when the root has focus.
 */
Window *get_focus_window(void);

/* Set the window that is in focus. */
void set_focus_window(Window *window);

/* Gives any window different from given window focus. */
void give_someone_else_focus(Window *window);

/* Get a window that is not shown but in the window list coming after
 * the given window or NULL when there is none.
 *
 * @window may be NULL.
 * @return NULL iff there is no hidden window.
 */
Window *get_next_hidden_window(Window *window);

/* Get a window that is not shown but in the window list coming before
 * the given window.
 *
 * @window may be NULL.
 * @return NULL iff there is no hidden window.
 */
Window *get_previous_hidden_window(Window *window);

/* -- Implemented in window_state.c -- */

/* Update the short_title of the window.
 */
void update_window_name(Window *window);

/* Update the size_hints of the window.
 */
void update_window_size_hints(Window *window);

/* Predict what state the window is expected to be in based on the X11
 * properties.
 */
unsigned predict_window_state(Window *window);

/* Change the state to given value and reconfigures the window.
 *
 * @force is used to force the change of the window state.
 */
void set_window_state(Window *window, unsigned state, unsigned force);

/* -- Implemented in tiling.c -- */

/* Split a frame horizontally or vertically. 
 * Set @is_split_vert to 1 for a vertical split and 0 for a horizontal split.
 */
void split_frame(Frame split_from, int is_split_vert);

/* Remove a frame from the screen and hide the inner window.
 *
 * This frame must have NO children.
 *
 * @return 1 when the given frame is the last frame, otherwise 0.
 */
int remove_leaf_frame(Frame frame);

/* -- Implemented in frame.c -- */

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame);

/* Check if the given point is within the given frame.
 *
 * @return 1 if the point is inside the frame, 0 otherwise */
int is_point_in_frame(Frame frame, int32_t x, int32_t y);

/* Get a frame at given position. */
Frame get_frame_at_position(int32_t x, int32_t y);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame frame);

/* Reposition the underlying window to fit within the frame.
 *
 * @frame may be NULL, then nothing happens.
 */
void reload_frame(Frame frame);

#endif
