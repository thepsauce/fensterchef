#ifndef WINDOW_STATE_H
#define WINDOW_STATE_H

/* all possible window states */
#define WINDOW_STATE_HIDDEN 0
#define WINDOW_STATE_SHOWN 1
#define WINDOW_STATE_POPUP 2
#define WINDOW_STATE_IGNORE 3
#define WINDOW_STATE_FULLSCREEN 4

/* HIDDEN: The window is not shown but would usually go in the tiling layout.
 * SHOWN: The window is part of the current tiling layout.
 * POPUP: The window is a popup window, it may or may not be visible.
 * IGNORE: The window was once registered by the WM but the user decided to
 *      ignore it. It does not appear when cycling through windows.
 * FULLSCREEN: The window covers an entire monitor.
 */

/* forward declaration */
struct window;
typedef struct window Window;

/* The state of the window signals our window manager what kind of window it is
 * and how the window should behave.
 */
typedef struct window_state {
    /* the current window state */
    unsigned current : 3;
    /* the previous window state */
    unsigned previous : 3;
    /* if the window was forced to be a certain state */
    unsigned is_forced : 1;
} WindowState;

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
