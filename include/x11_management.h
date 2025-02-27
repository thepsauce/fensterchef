#ifndef X11_MANAGEMENT_H
#define X11_MANAGEMENT_H

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h> // xcb_size_hints_t,
                           // xcb_icccm_wm_hints_t

#include "utf8.h"
#include "utility.h"
#include "window_state.h"

/* flag used to configure window position and size */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT)

/* expands to all atoms, this system with `X()` makes it easy to maintain
 *
 * atom types:
 * ATOM - an atom value itself
 * CARDINAL - an integer value
 * WINDOW - a window value
 * X[] - array of any size of type X
 * X[N] - array with N values of type X
 */
#define DEFINE_ALL_ATOMS \
    /* a utf8 string */ \
    X(UTF8_STRING) \
\
    /** ROOT WINDOW PROPERTIES (properties set on the root window) **/ \
    /* indicate the supported atoms */ \
    /* ATOM[] */ X(_NET_SUPPORTED) \
    /* used in `_NET_SUPPORTED` to indicate that our window manager does good
     * placement for windows
     */ \
    X(_NET_WM_FULL_PLACEMENT) \
    /* list of all visible windows, sent on request */ \
    /* WINDOW[] */ X(_NET_CLIENT_LIST) \
    /* list of all visible windows (in stacking order), sent on request */ \
    /* WINDOW[] */ X(_NET_CLIENT_LIST_STACKING) \
    /* number of desktops */ \
    /* CARDINAL */ X(_NET_NUMBER_OF_DESKTOPS) \
    /* common desktop size */ \
    /* CARDINAL[2] */ X(_NET_DESKTOP_GEOMETRY) \
    /* positions of the desktops */ \
    /* CARDINAL[][2] */ X(_NET_DESKTOP_VIEWPORT) \
    /* index of the current desktop */ \
    /* CARDINAL */ X(_NET_CURRENT_DESKTOP) \
    /* name of the desktops */ \
    /* UTF8_STRING[] */ X(_NET_DESKTOP_NAMES) \
    /* the currently active window */ \
    /* WINDOW */ X(_NET_ACTIVE_WINDOW) \
    /* the areas of the desktops */ \
    /* CARDINAL[][4] */ X(_NET_WORKAREA) \
    /* a special window indicating the window manager name */ \
    /* WINDOW */ X(_NET_SUPPORTING_WM_CHECK) \
    /* the virtual roots (our window manager does not operate with this) */ \
    /* WINDOW[] */ X(_NET_VIRTUAL_ROOTS) \
    /* set by a pager, not our window manager */ \
    /* CARDINAL[4] */ X(_NET_DESKTOP_LAYOUT) \
    /* set if the window manager is in showing desktop mode */ \
    /* CARDINAL */ X(_NET_SHOWING_DESKTOP) \
\
    /** ROOT WINDOW MESSAGES (sent to the root window via a ClientMessage) **/ \
    /* sent to close a window */ \
    X(_NET_CLOSE_WINDOW) \
    /* sent to move/resize a window */ \
    X(_NET_MOVERESIZE_WINDOW) \
    /* sent to move/resize a window dynamically (via user input) */ \
    X(_NET_WM_MOVERESIZE) \
    /* sent to change the window Z order */ \
    X(_NET_RESTACK_WINDOW) \
    /* sent to receive the estimatet frame extents */ \
    X(_NET_REQUEST_FRAME_EXTENTS) \
\
    /** WINDOW PROPERTIES (properties set on client windows) **/ \
    /* name of the window */ \
    /* UTF8_STRING */ X(_NET_WM_NAME) \
    /* name of the icon */ \
    /* UTF8_STRING */ X(_NET_WM_ICON_NAME) \
    /* the desktop the window is on */ \
    /* CARDINAL */ X(_NET_WM_DESKTOP) \
    /* list of window types `_NET_WM_WINDOW_TYPE_*` */ \
    /* ATOM[] */ X(_NET_WM_WINDOW_TYPE) \
    /* list of window states `_NET_WM_STATE_*` */ \
    /* ATOM[] */ X(_NET_WM_STATE) \
    /* many of `_NET_WM_ACTION_*` */ \
    /* ATOM[] */ X(_NET_WM_ALLOWED_ACTIONS) \
    /* reserved space */ \
    /* CARDINAL[4] */ X(_NET_WM_STRUT) \
    /* more precisely reserved space */ \
    /* CARDINAL[12] */ X(_NET_WM_STRUT_PARTIAL) \
    /* process id of the process where the window was created from */ \
    /* CARDINAL */ X(_NET_WM_PID) \
    /* set on a window to indicate the frame sizes */ \
    /* CARDINAL[4] */ X(_NET_FRAME_EXTENTS) \
    /* set on a window to indicate the fullscreen region */ \
    /* CARDINAL[4] */ X(_NET_WM_FULLSCREEN_MONITORS) \
    /* protocols a window supports */ \
    /* ATOM[] */ X(WM_PROTOCOLS) \
    /* the last time the user interacted with the window */ \
    /* CARDINAL */ X(_NET_WM_USER_TIME) \
    /* the name of the locale of the window, fox example: "en_US.UTF-8" */ \
    X(WM_LOCALE_NAME) \
    /* delete window message atom */ \
    X(WM_DELETE_WINDOW) \
    /* change state message atom */ \
    X(WM_CHANGE_STATE) \
    /** the window types set on `_NET_WM_WINDOW_TYPE` **/ \
    /* a destop window, usually covering the entire screen in the background
     */ \
    X(_NET_WM_WINDOW_TYPE_DESKTOP) \
    /* dock windows are attached to a side of a screen */ \
    X(_NET_WM_WINDOW_TYPE_DOCK) \
    /* pinnable window */ \
    X(_NET_WM_WINDOW_TYPE_TOOLBAR) \
    /* part of a toolbar */ \
    X(_NET_WM_WINDOW_TYPE_MENU) \
    /* temporary window like color palette */ \
    X(_NET_WM_WINDOW_TYPE_UTILITY) \
    /* start up window for some application */ \
    X(_NET_WM_WINDOW_TYPE_SPLASH) \
    /* dialog window */ \
    X(_NET_WM_WINDOW_TYPE_DIALOG) \
    /* dropdown menu */ \
    X(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU) \
    /* popup menu */ \
    X(_NET_WM_WINDOW_TYPE_POPUP_MENU) \
    /* tooltip */ \
    X(_NET_WM_WINDOW_TYPE_TOOLTIP) \
    /* notification window */ \
    X(_NET_WM_WINDOW_TYPE_NOTIFICATION) \
    /* combo box popup */ \
    X(_NET_WM_WINDOW_TYPE_COMBO) \
    /* object being dragged */ \
    X(_NET_WM_WINDOW_TYPE_DND) \
    /* normal window */ \
    X(_NET_WM_WINDOW_TYPE_NORMAL) \
    /** the window states set on `_NET_WM_STATE` **/ \
    /* the window is demanding sole focus */ \
    X(_NET_WM_STATE_MODAL) \
    /* the window is preserved across all desktops */ \
    X(_NET_WM_STATE_STICKY) \
    /* the window is maximized vertically */ \
    X(_NET_WM_STATE_MAXIMIZED_VERT) \
    /* the window is maximized horizontally */ \
    X(_NET_WM_STATE_MAXIMIZED_HORZ) \
    /* the window is shaded */ \
    X(_NET_WM_STATE_SHADED) \
    /* the window is not shown on a taskbar */ \
    X(_NET_WM_STATE_SKIP_TASKBAR) \
    /* the window is not included in a pager */ \
    X(_NET_WM_STATE_SKIP_PAGER) \
    /* the window is minimized */ \
    X(_NET_WM_STATE_HIDDEN) \
    /* the window is in fullscreen */ \
    X(_NET_WM_STATE_FULLSCREEN) \
    /* the window should be on top of all windows */ \
    X(_NET_WM_STATE_ABOVE) \
    /* the window should be below all windows */ \
    X(_NET_WM_STATE_BELOW) \
    /* the window should wants attention */ \
    X(_NET_WM_STATE_DEMANDS_ATTENTION) \
    /* the window is focused */ \
    X(_NET_WM_STATE_FOCUSED) \
    /** the allowed actions set on `_NET_WM_ALLOWED_ACTIONS` **/ \
    /* the window can move */ \
    X(_NET_WM_ACTION_MOVE) \
    /* the window can be resized */ \
    X(_NET_WM_ACTION_RESIZE) \
    /* the window can be minimized */ \
    X(_NET_WM_ACTION_MINIMIZE) \
    /* the window can be shaded */ \
    X(_NET_WM_ACTION_SHADE) \
    /* the window can be made sticky */ \
    X(_NET_WM_ACTION_STICK) \
    /* the window can be horizontally maximized */ \
    X(_NET_WM_ACTION_MAXIMIZE_HORZ) \
    /* the window can be vertically maximized */ \
    X(_NET_WM_ACTION_MAXIMIZE_VERT) \
    /* the window can be put into fullscreen */ \
    X(_NET_WM_ACTION_FULLSCREEN) \
    /* the window can change its desktop */ \
    X(_NET_WM_ACTION_CHANGE_DESKTOP) \
    /* the window can be closed */ \
    X(_NET_WM_ACTION_CLOSE) \
    /* the window may be put above all other windows */ \
    X(_NET_WM_ACTION_ABOVE) \
    /* the window may be put below all other windows */ \
    X(_NET_WM_ACTION_BELOW) \
\
    /* hints defined by the motif window manager */ \
    X(_MOTIF_WM_HINTS)

/* get the atom identifier from an atom constant */
#define ATOM(id) (x_atoms[id].atom)

/* constant list expansion of all atoms */
enum {
#define X(atom) atom,
    DEFINE_ALL_ATOMS
#undef X
    ATOM_MAX
};

/* all X atoms, initially all identifiers are 0 until `initialize_x11()` is
 * called
 */
extern struct x_atoms {
    /* name of the atom */
    const char *name;
    /* atom identifier */
    xcb_atom_t atom;
} x_atoms[ATOM_MAX];

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

/* cache of window properties */
typedef struct x_properties {
    /* the X window that has these properties */
    xcb_window_t window;

    /* window name */
    utf8_t *name;
    /* xcb size hints of the window */
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
    /* id of the process owning the window */
    uint32_t process_id;
    /* the motif window manager hints */
    motif_wm_hints_t motif_wm_hints;

    /* the window states */
    xcb_atom_t *states;
} XProperties;

/* connection to the X server */
extern xcb_connection_t *connection;

/* file descriptor associated to the X connection */
extern int x_file_descriptor;

/* general purpose values for xcb function calls */
extern uint32_t general_values[7];

/* the selected X screen */
extern xcb_screen_t *screen;

/* supporting wm check window */
extern xcb_window_t check_window;

/* user notification window */
extern xcb_window_t notification_window;

/* user window list window */
extern xcb_window_t window_list_window;

/* Check if given strut has any reserved space. */
static inline bool is_strut_empty(wm_strut_partial_t *strut)
{
    return strut->reserved.left == 0 && strut->reserved.top == 0 &&
        strut->reserved.right == 0 && strut->reserved.bottom == 0;
}

/* Initialize the X connection and the X atoms. */
int initialize_x11(void);

/* Try to take control of the window manager role (also initialize the
 * fensterchef windows).
 */
int take_control(void);

/* Initialize all properties within @properties and return the mode the window
 * should be in initially.
 */
window_mode_t initialize_window_properties(XProperties *properties,
        xcb_window_t window);

/* Update the property with @properties corresponding to given atom. */
bool cache_window_property(XProperties *properties, xcb_atom_t atom);

/* Check if @properties includes @protocol. */
bool supports_protocol(XProperties *properties, xcb_atom_t protocol);

/* Check if @properties includes @state. */
bool has_state(XProperties *properties, xcb_atom_t state);

/* Translate a string to a key symbol.
 *
 * *Implemented in string_to_keysym.c*
 */
xcb_keysym_t string_to_keysym(const char *string);

#endif
