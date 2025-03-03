#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include "bits/window_typedef.h"
#include "bits/frame_typedef.h"

#include "monitor.h"
#include "utility.h"
#include "window_state.h"

#include "x11_management.h"

/* the maximum width or height of the window */
#define WINDOW_MAXIMUM_SIZE 100000

/* the minimum width or height a window can have */
#define WINDOW_MINIMUM_SIZE 4

/* the minimum length of the window that needs to stay visible */
#define WINDOW_MINIMUM_VISIBLE_SIZE 8

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

/* A window is a wrapper around an X window, it is always part of a few global
 * linked list and has a unique id (number).
 */
struct window {
    /* the server's view of the window */
    XClient client;

    /* window name */
    utf8_t *name;

    /* X size hints of the window */
    xcb_size_hints_t size_hints;

    /* special window manager hints */
    xcb_icccm_wm_hints_t hints;

    /* window strut (reserved region on the screen) */
    wm_strut_partial_t strut;

    /* the window this window is transient for */
    xcb_window_t transient_for;

    /* the protocols the window supports */
    xcb_atom_t *protocols;

    /* the region the window should appear at as fullscreen window */
    Extents fullscreen_monitors;

    /* the motif window manager hints */
    motif_wm_hints_t motif_wm_hints;

    /* the window states */
    xcb_atom_t *states;

    /* the window state */
    WindowState state;

    /* current window position and size */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* size and color of the border */
    uint32_t border_size;
    uint32_t border_color;

    /* position and size when the window was in floating mode */
    Rectangle floating;

    /* the id of this window */
    uint32_t number;

    /* All windows are part of the Z ordered linked list even when they are
     * hidden now.
     *
     * The terms Z stack, Z linked list and Z stacking are used interchangeably.
     */
    /* the window above this window */
    Window *below;
    /* the window below this window */
    Window *above;

    /* The age linked list stores the windows in creation time order. */
    /* a window newer than this one */
    Window *newer;

    /* The number linked list stores the windows sorted by their number. */
    /* the next window in the linked list */
    Window *next;
};

/* the window that was created before any other */
extern Window *oldest_window;

/* the window at the bottom of the Z stack */
extern Window *bottom_window;

/* the window at the top of the Z stack */
extern Window *top_window;

/* the first window in the number linked list */
extern Window *first_window;

/* the currently focused window */
extern Window *focus_window;

/* Create a window struct and add it to the window list. */
Window *create_window(xcb_window_t xcb);

/* time in seconds to wait for a second close */
#define REQUEST_CLOSE_MAX_DURATION 3

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(Window *window);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying xcb window.
 */
void destroy_window(Window *window);

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int32_t *x, int32_t *y,
        uint32_t width, uint32_t height, uint32_t window_gravity);

/* Move the window such that it is in bounds of @monitor.
 *
 * @monitor may be NULL, then a monitor is chosen.
 */
void place_window_in_bounds(Monitor *monitor, Window *window);

/* Get the minimum size the window should have. */
void get_minimum_window_size(const Window *window, Size *size);

/* Get the maximum size the window should have. */
void get_maximum_window_size(const Window *window, Size *size);

/* Set the position and size of a window.
 *
 * Note that this function clips the parameters using `get_minimum_size()` and
 * `get_maximum_size()`.
 */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height);

/* Put the window on the best suited Z stack position. */
void update_window_layer(Window *window);

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

/* Check if the window accepts input focus. */
bool does_window_accept_focus(Window *window);

/* Set the window that is in focus. */
void set_focus_window(Window *window);

#endif
