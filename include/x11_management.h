#ifndef X11_MANAGEMENT_H
#define X11_MANAGEMENT_H

#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "bits/window.h"
#include "utility/utility.h"

/* needed for `_NET_WM_STRUT_PARTIAL`/`_NET_WM_STRUT` */
typedef struct wm_strut_partial {
    /* reserved space on left border */
    int left;
    /* reserved space on right border */
    int right;
    /* reserved space on top border */
    int top;
    /* reserved space on bottom border */
    int bottom;
    /* beginning y coordinate of the left strut */
    int left_start_y;
    /* ending y coordinate of the left strut */
    int left_end_y;
    /* beginning y coordinate of the right strut */
    int right_start_y;
    /* ending y coordinate of the right strut */
    int right_end_y;
    /* beginning x coordinate of the top strut */
    int top_start_x;
    /* ending x coordinate of the top strut */
    int top_end_x;
    /* beginning x coordinate of the bottom strut */
    int bottom_start_x;
    /* ending x coordinate of the bottom strut */
    int bottom_end_x;
} wm_strut_partial_t;

/* `_NET_WM_MOVERESIZE` window movement or resizing */
typedef enum {
    /* resizing applied on the top left edge */
    _NET_WM_MOVERESIZE_SIZE_TOPLEFT = 0,
    /* resizing applied on the top edge */
    _NET_WM_MOVERESIZE_SIZE_TOP = 1,
    /* resizing applied on the top right edge */
    _NET_WM_MOVERESIZE_SIZE_TOPRIGHT = 2,
    /* resizing applied on the right edge */
    _NET_WM_MOVERESIZE_SIZE_RIGHT = 3,
    /* resizing applied on the bottom right edge */
    _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT = 4,
    /* resizing applied on the bottom edge */
    _NET_WM_MOVERESIZE_SIZE_BOTTOM = 5,
    /* resizing applied on the bottom left edge */
    _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT = 6,
    /* resizing applied on the left edge */
    _NET_WM_MOVERESIZE_SIZE_LEFT = 7,
    /* movement only */
    _NET_WM_MOVERESIZE_MOVE = 8,
    /* size via keyboard */
    _NET_WM_MOVERESIZE_SIZE_KEYBOARD = 9,
    /* move via keyboard */
    _NET_WM_MOVERESIZE_MOVE_KEYBOARD = 10,
    /* cancel operation */
    _NET_WM_MOVERESIZE_CANCEL = 11,
    /* automatically figure out a good direction */
    _NET_WM_MOVERESIZE_AUTO,
} wm_move_resize_direction_t;

/* `_NET_WM_STATE` state change */
/* a state should be removed */
#define _NET_WM_STATE_REMOVE 0
/* a state should be added */
#define _NET_WM_STATE_ADD 1
/* a state should be toggled (removed if it exists and added otherwise) */
#define _NET_WM_STATE_TOGGLE 2

typedef struct x_client {
    /* the id of the window */
    Window id;
    /* if the window is mapped (visible) */
    bool is_mapped;
    /* position and size of the window */
    int x;
    int y;
    unsigned width;
    unsigned height;
    /* the size of the border */
    unsigned int border_width;
    /* the color of the border */
    uint32_t border;
} XClient;

/* supporting wm check window */
extern Window wm_check_window;

/* file descriptor associated to the X display */
extern int x_file_descriptor;

/* Check if given strut has any reserved space. */
static inline bool is_strut_empty(wm_strut_partial_t *strut)
{
    return strut->left == 0 && strut->top == 0 &&
        strut->right == 0 && strut->bottom == 0;
}

/* Try to take control of the window manager role.
 *
 * Call this after `initialize_connection()`. This will set
 * `Fensterchef_is_running` to true if it succeeds.
 *
 * If it fails, it aborts fensterchef.
 */
void take_control(void);

/* Go through all already existing windows and manage them.
 *
 * Call this after `initialize_monitors()`.
 */
void query_existing_windows(void);

/* Set the input focus to @window. This window may be `NULL`. */
void set_input_focus(FcWindow *window);

/* Show the client on the X server. */
void map_client(XClient *client);

/* Show the client on the X server at the top. */
void map_client_raised(XClient *client);

/* Hide the client on the X server. */
void unmap_client(XClient *client);

/* Set the size of a window associated to the X server. */
void configure_client(XClient *client, int x, int y, unsigned width,
        unsigned height, unsigned border_width);

/* Set the background and border color of @client. */
void change_client_attributes(XClient *client, uint32_t border_color);

#endif
