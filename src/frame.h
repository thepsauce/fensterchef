#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#include <xcb/xcb.h>

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

typedef struct window {
    /* the actual X window */
    xcb_window_t xcb_window;
    /* if the window is visible */
    unsigned visible : 1;
    /* if the window has focus */
    unsigned focused : 1;
    /* the id of this window */
    uint32_t number;
    /* the next window in the linked list */
    struct window *next;
} Window;

/* the first window in the linked list */
extern Window *g_first_window;

typedef struct frame {
    /* the window inside the frame, may be NULL */
    Window *window;
    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    /* the next frame in the linked list */
    struct frame *next;
} Frame;

/* the first frame in the linked list */
extern Frame *g_first_frame;
/* the currently selected/focused frame */
extern Frame *g_cur_frame;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window);

/* Get the internal window that has the associated xcb window.
 *
 * @return NULL when none has this xcb window.
 */
Window *get_window_of_xcb_window(xcb_window_t xcb_window);

/* Get the frame this window is contained in.
 *
 * @return NULL when the window is not in any frame.
 */
Frame *get_frame_of_window(Window *window);

/* Show the window by mapping and sizing it. */
void show_window(Window *window);

/* Hide the window by unmapping it. */
void hide_window(Window *window);

/* Get the currently focused window.
 *
 * @return NULL when the root has focus.
 */
Window *get_focus_window(void);

/* Set the window that is in focus. */
void set_focus_window(Window *window);

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
Window *get_prev_hidden_window(Window *window);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying xcb window.
 */
void destroy_window(Window *window);

/* Create a frame at given coordinates that contains a window
 * and attach it to the linked list.
 *
 * @window may be NULL to indicate an empty frame.
 */
Frame *create_frame(Window *window, int32_t x, int32_t y, int32_t w, int32_t h);

/* Remove a frame from the screen and hide the inner window.
 *
 * @return 1 when the given frame is the last frame, otherwise 0.
 */
int remove_frame(Frame *frame);

/* Set the frame in focus, this also focuses the inner window if it exists. */
void set_focus_frame(Frame *frame);

#endif
