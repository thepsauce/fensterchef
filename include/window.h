#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include <fontconfig/fontconfig.h> // FcChar8

#include <xcb/xcb.h> // xcb_window_t
#include <xcb/xcb_icccm.h> // xcb_size_hints_t, xcb_icccm_wm_hints_t

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
    /* special window manager hints */
    xcb_icccm_wm_hints_t wm_hints;
    /* short window title */
    FcChar8 short_title[256];
    /* size when the window was in popup state */
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
extern Window *g_first_window;

/* Create a window struct and add it to the window list,
 * this also assigns the next id. */
Window *create_window(xcb_window_t xcb_window);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying xcb window.
 */
void destroy_window(Window *window);

/* Update the short_title of the window. */
void update_window_name(Window *window);

/* Update the size_hints of the window. */
void update_window_size_hints(Window *window);

/* Update the wm_hints of the window. */
void update_window_wm_hints(Window *window);

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

/* -- Implemented in window_state.c -- */

/* Predict what state the window is expected to be in based on the X11
 * properties.
 */
unsigned predict_window_state(Window *window);

/* Change the state to given value and reconfigures the window.
 *
 * @force is used to force the change of the window state.
 */
void set_window_state(Window *window, unsigned state, unsigned force);

#endif
