#ifndef WINDOW_STATE_H
#define WINDOW_STATE_H

#include <stdbool.h>
#include <time.h>

#include "bits/window.h"

/* the mode of the window */
typedef enum window_mode {
    /* the window is part of the tiling layout (if visible) */
    WINDOW_MODE_TILING,
    /* the window is a floating window */
    WINDOW_MODE_FLOATING,
    /* the window is a fullscreen window */
    WINDOW_MODE_FULLSCREEN,
    /* the window is attached to an edge of the screen, usually not focusable */
    WINDOW_MODE_DOCK,
    /* the window is in the background */
    WINDOW_MODE_DESKTOP,

    /* the maximum value of a window mode */
    WINDOW_MODE_MAX,
} window_mode_t;

/* The state of the window signals our window manager what kind of window it is
 * and how the window should behave.
 */
typedef struct window_state {
    /* if the window is visible (mapped) */
    bool is_visible;
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
window_mode_t predict_window_mode(FcWindow *window);

/* Check if @window should have a border. */
bool is_window_borderless(FcWindow *window);

/* Get the side of a monitor @window would like to attach to.
 *
 * This is based on the window strut if it is set, otherwise the gravity in
 * `size_hints` is used.
 *
 * @return StaticGravity if there is no preference.
 */
int get_window_gravity(FcWindow *window);

/* Set the position and size of the window to a dock window. */
void configure_dock_size(FcWindow *window);

/* Add window states to the window's properties. */
void add_window_states(FcWindow *window, Atom *states,
        unsigned number_of_states);

/* Remove window states from the window's properties. */
void remove_window_states(FcWindow *window, Atom *states,
        unsigned number_of_states);

/* Change the mode to given value and reconfigures the window if it is visible.
 *
 * @force_mode is used to force the change of the window mode.
 */
void set_window_mode(FcWindow *window, window_mode_t mode);

/* Show the window by positioning it and mapping it to the X server.
 *
 * Note that this removes the given window from the taken window list.
 */
void show_window(FcWindow *window);

/* Hide @window and adjust the tiling and focus.
 *
 * When the window is a tiling window, this places the next available window in
 * the formed gap.
 *
 * The next window is focused.
 */
void hide_window(FcWindow *window);

/* Hide the window without touching the tiling or focus.
 *
 * Note: The focus however is removed if @window is the focus.
 *
 * To abrubtly show a window, simply do: `window->state.is_visible = true`.
 */
void hide_window_abruptly(FcWindow *window);

#endif
