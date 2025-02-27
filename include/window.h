#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include "bits/window_typedef.h"
#include "bits/frame_typedef.h"

#include "monitor.h"
#include "utility.h"
#include "window_state.h"

#include "x11_management.h"

/* the maximum size of the window */
#define WINDOW_MAXIMUM_SIZE 1000000

/* the minimum length of the window that needs to stay visible */
#define WINDOW_MINIMUM_VISIBLE_SIZE 8

/* the minimum width or height a window can have */
#define WINDOW_MINIMUM_SIZE 4

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

/* A window is a wrapper around an xcb window, it is always part of a global
 * linked list and has a unique id.
 */
struct window {
    /* the window's X properties */
    XProperties properties;

    /* the time the window was created */
    time_t creation_time;

    /* the window state */
    WindowState state;

    /* current window position and size */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    /* position and size when the window was in floating mode */
    Rectangle floating;

    /* the id of this window */
    uint32_t number;

    /* All windows are part of the Z ordered linked list even when they are
     * hidden now.
     */
    /* the window above this window */
    Window *below;
    /* the window below this window */
    Window *above;

    /* the next window in the linked list (sorted by the window number) */
    Window *next;
};

/* the first window in the linked list, the list is sorted increasingly
 * with respect to the window number
 */
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

/* Set the position and size of a window. */
void set_window_size(Window *window, int32_t x, int32_t y, uint32_t width,
        uint32_t height);

/* Get the window below all other windows. */
Window *get_bottom_window(void);

/* Get the window on top of all other windows. */
Window *get_top_window(void);

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
