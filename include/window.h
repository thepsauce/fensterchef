#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include <xcb/xcb.h> // xcb_window_t

#include "util.h"

#include "window_properties.h"
#include "window_state.h"

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

/* A window is a wrapper around an xcb window, it is always part of a global
 * linked list and has a unique id.
 */
typedef struct window {
    /* the actual X window */
    xcb_window_t xcb_window;

    /* frame this window is contained in */
    struct frame *frame;

    /* the window properties */
    WindowProperties properties;

    /* current window position and size */
    Position position;
    Size size;

    /* position and size when the window was in popup state */
    Position popup_position;
    Size popup_size;

    /* the window state */
    WindowState state;

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
extern Window *g_first_window;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying xcb window.
 */
void destroy_window(Window *window);

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height);

/* Put the window on top of all other windows. */
void set_window_above(Window *window);

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
Frame *get_frame_of_window(const Window *window);

/* Get the currently focused window.
 *
 * @return NULL when the root has focus.
 */
Window *get_focus_window(void);

/* Set the window that is in focus.
 *
 * @return 1 if the window does not accept focus, 0 otherwise.
 */
int set_focus_window(Window *window);

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

/* Puts a window into a frame and matches its size.
 *
 * This also disconnects the frame of the window AND the window of the frame
 * from their respective frame and window which makes this a very safe function.
 */
void link_window_and_frame(Window *window, Frame *frame);

#endif
