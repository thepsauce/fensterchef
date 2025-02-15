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
    /* the window is attached to an edge of the screen, usually not focusable */
    WINDOW_MODE_DOCK,

    /* the maximum value of a window mode */
    WINDOW_MODE_MAX,
} window_mode_t;

#define DOES_WINDOW_MODE_HAVE_BORDER(mode) \
    ((mode) == WINDOW_MODE_TILING || (mode) == WINDOW_MODE_POPUP)

/* forward declaration */
struct window;
typedef struct window Window;

/* The state of the window signals our window manager what kind of window it is
 * and how the window should behave.
 */
typedef struct window_state {
    /* if the window was ever mapped */
    bool was_ever_mapped;
    /* if the window is visible (mapped) */
    bool is_visible;
    /* if the window was forced to be a certain mode */
    bool is_mode_forced;
    /* the current window state */
    window_mode_t mode;
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

/* Show given window but do not assign an id and no size configuring. */
void show_window_quickly(Window *window);

/* Show the window by positioning it and mapping it to the X server. */
void show_window(Window *window);

/* Hide a window without focusing another window or filling the tiling gap.
 *
 * The caller must make sure to give another window focus if the given window is
 * the focus window.
 */
void hide_window_quickly(Window *window);

/* Hide the window by unmapping it from the X server.
 *
 * When the window is a tiling window, this places the next available window in
 * the formed gap.
 *
 * The next window is focused.
 */
void hide_window(Window *window);

#endif
