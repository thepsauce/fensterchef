#ifndef WINDOW_STATE_H
#define WINDOW_STATE_H

#include <stdbool.h>

#include "bits/window_typedef.h"

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
    /* if the user ever requested to close the window */
    bool was_close_requested;
    /* when the user requested to close the window */
    time_t user_request_close_time;
    /* the current window mode */
    window_mode_t mode;
    /* the previous window mode */
    window_mode_t previous_mode;
} WindowState;

/* Predict what mode the window is expected to be in based on the X
 * properties.
 */
window_mode_t predict_window_mode(Window *window);

/* Check if @window has a visible border currently. */
bool has_window_border(Window *window);

/* Change the mode to given value and reconfigures the window if it is visible.
 *
 * @force_mode is used to force the change of the window mode.
 */
void set_window_mode(Window *window, window_mode_t mode, bool force_mode);

/* Show the window by positioning it and mapping it to the X server.
 *
 * Note that this removes the given window from the taken window list.
 */
void show_window(Window *window);

/* Hide the window by unmapping it from the X server.
 *
 * When the window is a tiling window, this places the next available window in
 * the formed gap.
 *
 * The next window is focused.
 */
void hide_window(Window *window);

/* Wrapper around `hide_window()` that does not touch the tiling or focus.
 *
 * Note: The focus however is removed if @window is the focus.
 */
void hide_window_abruptly(Window *window);

#endif
