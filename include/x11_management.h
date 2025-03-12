#ifndef X11_MANAGEMENT_H
#define X11_MANAGEMENT_H

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h> // xcb_size_hints_t,
                           // xcb_icccm_wm_hints_t

#include "bits/window_typedef.h"

#include "utf8.h"
#include "utility.h"

/* needed for `_NET_WM_STRUT_PARTIAL`/`_NET_WM_STRUT` */
typedef struct wm_strut_partial {
    /* reserved space on the border of the screen */
    Extents reserved;
    /* beginning y coordinate of the left strut */
    uint32_t left_start_y;
    /* ending y coordinate of the left strut */
    uint32_t left_end_y;
    /* beginning y coordinate of the right strut */
    uint32_t right_start_y;
    /* ending y coordinate of the right strut */
    uint32_t right_end_y;
    /* beginning x coordinate of the top strut */
    uint32_t top_start_x;
    /* ending x coordinate of the top strut */
    uint32_t top_end_x;
    /* beginning x coordinate of the bottom strut */
    uint32_t bottom_start_x;
    /* ending x coordinate of the bottom strut */
    uint32_t bottom_end_x;
} wm_strut_partial_t;

/* whether the window has decorations */
#define MOTIF_WM_HINTS_DECORATIONS (1 << 1)

/* needed for a`_MOTIF_WM_HINTS` to determine if a window wants to hide borders
 */
typedef struct motif_wm_hints {
    /* what fields below are available */
    uint32_t flags;
    /* IGNORED */
    uint32_t functions;
    /* IGNORED */
    uint32_t decorations;
    /* IGNORED */
    uint32_t input_mode;
    /* IGNORED */
    uint32_t status;
} motif_wm_hints_t;

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
    xcb_window_t id;
    /* if the window is mapped (visible) */
    bool is_mapped;
    /* position and size of the window */
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    /* the size of the border */
    uint32_t border_width;
    /* the color of the border */
    uint32_t border_color;
} XClient;

/* connection to the X server */
extern xcb_connection_t *connection;

/* file descriptor associated to the X connection */
extern int x_file_descriptor;

/* general purpose values for xcb function calls */
extern uint32_t general_values[7];

/* the selected X screen */
extern xcb_screen_t *screen;

/* supporting wm check window */
extern xcb_window_t wm_check_window;

/* user notification window */
extern XClient notification;

/* Check if given strut has any reserved space. */
static inline bool is_strut_empty(wm_strut_partial_t *strut)
{
    return strut->reserved.left == 0 && strut->reserved.top == 0 &&
        strut->reserved.right == 0 && strut->reserved.bottom == 0;
}

/* Initialize the X connection. */
int initialize_x11(void);

/* Try to take control of the window manager role (also initialize the
 * fensterchef windows).
 *
 * Call this after `initialize_x11()`
 */
int take_control(void);

/* Go through all already existing windows and manage them.
 *
 * Call this after `initialize_monitors()`.
 */
void query_existing_windows(void);

/* Set the input focus to @window. This window may be `NULL`. */
void set_input_focus(Window *window);

/* Show the client on the X server. */
void map_client(XClient *client);

/* Hide the client on the X server. */
void unmap_client(XClient *client);

/* Set the size of a window associated to the X server. */
void configure_client(XClient *client, int32_t x, int32_t y, uint32_t width,
        uint32_t height, uint32_t border_width);

/* Set the border color of @client. */
void change_client_attributes(XClient *client, uint32_t border_color);

/* Translate a string to a key symbol.
 *
 * *Implemented in string_to_keysym.c*
 */
xcb_keysym_t string_to_keysym(const char *string);

#endif
