#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include "bits/window_typedef.h"
#include "bits/frame_typedef.h"

#include "configuration/configuration.h"
#include "monitor.h"
#include "utility.h"
#include "window_state.h"

#include "x11_management.h"

/* the maximum width or height of a window */
#define WINDOW_MAXIMUM_SIZE UINT16_MAX

/* the minimum width or height a window can have */
#define WINDOW_MINIMUM_SIZE 4

/* the minimum length of the window that needs to stay visible */
#define WINDOW_MINIMUM_VISIBLE_SIZE 8

/* A window is a wrapper around an X window, it is always part of a few global
 * linked list and has a unique id (number).
 */
struct window {
    /* reference counter to keep the pointer around for longer after the window
     * has been destroyed; a destroyed but still referenced window will have
     * `client.id` set to `XCB_NONE`, all other struct members are invalid
     */
    uint32_t reference_count;

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

    /* the window states containing atoms `_NET_WM_STATE_*` */
    xcb_atom_t *states;

    /* the current `WM_STATE` atom set on the window, either
     * `WM_STATE_NORMAL` or `WM_STATE_WITHDRAWN`
     */
    xcb_atom_t wm_state;

    /* the window state */
    WindowState state;

    /* current window position and size */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* if the window should have no border as floating window; to check if a
     * window *actually* has no border, use `is_window_borderless()`
     */
    bool is_borderless;
    /* size and color of the border */
    uint32_t border_size;
    uint32_t border_color;

    /* position/size when the window was in floating mode */
    Rectangle floating;

    /* the number of this window, multiple windows may have the same number */
    uint32_t number;

    /* The age linked list stores the windows in creation time order. */
    /* a window newer than this one */
    Window *newer;

    /* All windows are part of the Z ordered linked list even when they are
     * hidden now.
     *
     * The terms Z stack, Z linked list and Z stacking are used interchangeably.
     */
    /* the window above this window */
    Window *below;
    /* the window below this window */
    Window *above;

    /* The number linked list stores the windows sorted by their number. */
    /* the next window in the linked list */
    Window *next;
};

/* the number of all windows within the linked list, this value is kept up to
 * date through `create_window()` and `destroy_window()`
 */
extern uint32_t Window_count;

/* the window that was created before any other */
extern Window *Window_oldest;

/* the window at the bottom of the Z stack */
extern Window *Window_bottom;

/* the window at the top of the Z stack */
extern Window *Window_top;

/* the first window in the number linked list */
extern Window *Window_first;

/* the currently focused window */
extern Window *Window_focus;

/* the last pressed window, this only gets set when a window is pressed by a
 * grabbed button
 */
extern Window *Window_pressed;

/* Increment the reference count of the window. */
void reference_window(Window *window);

/* Decrement the reference count of the window and free @window when it reaches
 * 0.
 */
void dereference_window(Window *window);

/* Create a window object and add it to all window lists.
 *
 * @association is filled with any associations found belonging to the window.
 */
Window *create_window(xcb_window_t xcb,
        struct configuration_association *association);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying X window.
 */
void destroy_window(Window *window);

/* Get a window with given @id or NULL if no window has that id. */
Window *get_window_by_id(uint32_t id);

/* time in seconds to wait for a second close */
#define REQUEST_CLOSE_MAX_DURATION 2

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(Window *window);

/* Adjust given @x and @y such that it follows the @window_gravity. */
void adjust_for_window_gravity(Monitor *monitor, int32_t *x, int32_t *y,
        uint32_t width, uint32_t height, xcb_gravity_t gravity);

/* Move the window such that it is in bounds of the screen. */
void place_window_in_bounds(Window *window);

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

/* Get the internal window that has the associated X window.
 *
 * @return NULL when none has this X window.
 */
Window *get_window_of_xcb_window(xcb_window_t xcb_window);

/* Get the frame this window is contained in.
 *
 * @return NULL when the window is not in any frame.
 */
Frame *get_frame_of_window(const Window *window);

/* Check if the window accepts input focus. */
bool is_window_focusable(Window *window);

/* Set the window that is in focus. */
void set_focus_window(Window *window);

#endif
