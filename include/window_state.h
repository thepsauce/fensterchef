#ifndef WINDOW_STATE_H
#define WINDOW_STATE_H

/* the mode of the window */
typedef enum window_mode {
    /* the window is part of the tiling layout (if visible) */
    WINDOW_MODE_TILING,
    /* the window is a popup window */
    WINDOW_MODE_POPUP,
    /* the window is a fullscreen window */
    WINDOW_MODE_FULLSCREEN,
} window_mode_t;

/* forward declaration */
struct window;
typedef struct window Window;

/* The state of the window signals our window manager what kind of window it is
 * and how the window should behave.
 */
typedef struct window_state {
    /* if the window was ever mapped */
    bool is_mappable;
    /* if the window is visible (mapped) */
    bool is_visible;
    /* if the window was forced to be a certain mode */
    bool is_mode_forced;
    /* the current window state */
    window_mode_t current_mode;
    /* the previous window state */
    window_mode_t previous_mode;
} WindowState;

/* Predict what mode the window is expected to be in based on the X
 * properties.
 */
window_mode_t predict_window_mode(Window *window);

/* Change the mode to given value and reconfigures the window if it is visible.
 *
 * @force_mode is used to force the change of the window mode.
 */
void set_window_mode(Window *window, window_mode_t mode, bool force_mode);

/* Show the window by positioning it and mapping it to the X server. */
void show_window(Window *window);

/* Hide the window by unmapping it to the X server. */
void hide_window(Window *window);

#endif
