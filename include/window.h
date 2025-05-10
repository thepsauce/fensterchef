#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include "bits/window.h"
#include "bits/frame.h"
#include "configuration.h"
#include "monitor.h"
#include "utility/utility.h"
#include "window_state.h"

#include "x11_management.h"

/* the maximum width or height of a window */
#define WINDOW_MAXIMUM_SIZE UINT16_MAX

/* the minimum width or height a window can have */
#define WINDOW_MINIMUM_SIZE 4

/* the minimum length of the window that needs to stay visible */
#define WINDOW_MINIMUM_VISIBLE_SIZE 8

/* association between class/instance and actions */
struct window_association {
    /* the pattern the instance should match, may be NULL which implies its
     * value is '*'
     */
    _Nullable utf8_t *instance_pattern;
    /* the pattern the class should match */
    utf8_t *class_pattern;
    /* the actions to execute */
    struct action_list actions;
};

/* A window is a wrapper around an X window, it is always part of a few global
 * linked list and has a unique id (number).
 */
struct fensterchef_window {
    /* reference counter to keep the pointer around for longer after the window
     * has been destroyed; a destroyed but still referenced window will have
     * `client.id` set to `None`, all other struct members are invalid
     */
    unsigned reference_count;

    /* the server's view of the window */
    XClient client;

    /* window name */
    utf8_t *name;

    /* window instance */
    utf8_t *instance;
    /* window class */
    utf8_t *class;

    /* X size hints of the window */
    XSizeHints size_hints;

    /* special window manager hints */
    XWMHints hints;

    /* window strut (reserved region on the screen) */
    wm_strut_partial_t strut;

    /* the window this window is transient for */
    Window transient_for;

    /* the protocols the window supports */
    Atom *protocols;

    /* the region the window should appear at as fullscreen window */
    Extents fullscreen_monitors;

    /* the window states containing atoms `_NET_WM_STATE_*` */
    Atom *states;

    /* the current `WM_STATE` atom set on the window, either
     * `WM_STATE_NORMAL` or `WM_STATE_WITHDRAWN`
     */
    Atom wm_state;

    /* the window state */
    WindowState state;

    /* current window position and size */
    int x;
    int y;
    unsigned int width;
    unsigned int height;

    /* if the window should have no border as floating window; to check if a
     * window *actually* has no border, use `is_window_borderless()`
     */
    bool is_borderless;
    /* size and color of the border */
    unsigned border_size;
    unsigned border_color;

    /* position/size when the window was in floating mode */
    Rectangle floating;

    /* the number of this window, multiple windows may have the same number */
    unsigned number;

    /* The age linked list stores the windows in creation time order. */
    /* a window newer than this one */
    FcWindow *newer;

    /* All windows are part of the Z ordered linked list even when they are
     * hidden now.
     *
     * The terms Z stack, Z linked list and Z stacking are used interchangeably.
     *
     * There is a second linked list to store the server state.  This is not
     * updated by the window module but the synchronization function.
     */
    /* the window above this window */
    FcWindow *below;
    /* the window below this window */
    FcWindow *above;
    /* the window that is below on the actual server side */
    FcWindow *server_below;
    /* the window that is above on the actual server side */
    FcWindow *server_above;

    /* The number linked list stores the windows sorted by their number. */
    /* the next window in the linked list */
    FcWindow *next;
};

/* the number of all windows within the linked list, this value is kept up to
 * date through `create_window()` and `destroy_window()`
 */
extern unsigned Window_count;

/* the window that was created before any other */
extern FcWindow *Window_oldest;

/* the window at the bottom of the Z stack */
extern FcWindow *Window_bottom;

/* the window at the top of the Z stack */
extern FcWindow *Window_top;

/* the first window in the number linked list */
extern FcWindow *Window_first;

/* The window at the top of the Z stack on the server.  We do not need the
 * bottom one, it is simply not needed.
 */
extern FcWindow *Window_server_top;

/* the currently focused window */
extern FcWindow *Window_focus;

/* the last pressed window, this only gets set when a window is pressed by a
 * grabbed button or when an association runs
 */
extern FcWindow *Window_pressed;

/* Increment the reference count of the window. */
void reference_window(FcWindow *window);

/* Decrement the reference count of the window and free @window when it reaches
 * 0.
 */
void dereference_window(FcWindow *window);

/* Add a new association from window instance/class to actions.
 *
 * This function takes control of all memory within @associations.
 * Do NOT free it.
 */
void add_window_associations(struct window_association *associations,
        unsigned number_of_associations);

/* Run the action associated to given window.
 *
 * @return true if any association existed, false otherwise.
 */
bool run_window_association(FcWindow *window);

/* Clear all currently set window associations. */
void clear_window_associations(void);

/* Create a window object and add it to all window lists.
 *
 * This also runs any associated actions or does the default behavior of showing
 * the window.
 */
FcWindow *create_window(Window id);

/* Destroy given window and removes it from the window linked list.
 * This does NOT destroy the underlying X window.
 */
void destroy_window(FcWindow *window);

/* Get a window with given @id or NULL if no window has that id. */
FcWindow *get_window_by_number(unsigned id);

/* time in seconds to wait for a second close */
#define REQUEST_CLOSE_MAX_DURATION 2

/* Attempt to close a window. If it is the first time, use a friendly method by
 * sending a close request to the window. Call this function again within
 * `REQUEST_CLOSE_MAX_DURATION` to forcefully kill it.
 */
void close_window(FcWindow *window);

/* Adjust given @x and @y such that it follows the @gravity. */
void adjust_for_window_gravity(Monitor *monitor, int *x, int *y,
        unsigned int width, unsigned int height, int gravity);

/* Get the minimum size the window should have. */
void get_minimum_window_size(const FcWindow *window, Size *size);

/* Get the maximum size the window should have. */
void get_maximum_window_size(const FcWindow *window, Size *size);

/* Set the position and size of a window.
 *
 * Note that this function clips the parameters using `get_minimum_size()` and
 * `get_maximum_size()`.
 */
void set_window_size(FcWindow *window, int x, int y, unsigned int width,
        unsigned int height);

/* Get the internal window that has the associated X window.
 *
 * @return NULL when none has this X window.
 */
FcWindow *get_fensterchef_window(Window id);

/* Get the frame this window is contained in.
 *
 * @return NULL when the window is not in any frame.
 */
Frame *get_window_frame(const FcWindow *window);

/* Check if the window accepts input focus. */
bool is_window_focusable(FcWindow *window);

/* Set the window that is in focus. */
void set_focus_window(FcWindow *window);

/* Focus @window and the frame it is contained in if any. */
void set_focus_window_with_frame(FcWindow *window);

#endif
