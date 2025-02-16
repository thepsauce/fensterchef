#ifndef X_H
#define X_H

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h> // xcb_size_hints_t,
                           // xcb_icccm_wm_hints_t

#include "utf8.h"
#include "utility.h"

/* flag used to configure window position, size and border width */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT)

/* expands to all atoms, this system with `X()` makes it easy to maintain */
#define DEFINE_ALL_ATOMS \
    X(_NET_SUPPORTED) \
    X(_NET_CLIENT_LIST) \
    X(_NET_CLIENT_LIST_STACKING) \
    X(_NET_NUMBER_OF_DESKTOPS) \
    X(_NET_DESKTOP_GEOMETRY) \
    X(_NET_DESKTOP_VIEWPORT) \
    X(_NET_CURRENT_DESKTOP) \
    X(_NET_DESKTOP_NAMES) \
    X(_NET_ACTIVE_WINDOW) \
    X(_NET_WORKAREA) \
    X(_NET_SUPPORTING_WM_CHECK) \
    X(_NET_VIRTUAL_ROOTS) \
    X(_NET_DESKTOP_LAYOUT) \
    X(_NET_SHOWING_DESKTOP) \
    X(_NET_CLOSE_WINDOW) \
    X(_NET_MOVERESIZE_WINDOW) \
    X(_NET_WM_MOVERESIZE) \
    X(_NET_RESTACK_WINDOW) \
    X(_NET_REQUEST_FRAME_EXTENTS) \
    X(_NET_WM_NAME) \
    X(_NET_WM_VISIBLE_NAME) \
    X(_NET_WM_ICON_NAME) \
    X(_NET_WM_VISIBLE_ICON_NAME) \
    X(_NET_WM_DESKTOP) \
    X(_NET_WM_WINDOW_TYPE) \
    X(_NET_WM_STATE) \
    X(_NET_WM_ALLOWED_ACTIONS) \
    X(_NET_WM_STRUT) \
    X(_NET_WM_STRUT_PARTIAL) \
    X(_NET_WM_ICON_GEOMETRY) \
    X(_NET_WM_ICON) \
    X(_NET_WM_PID) \
    X(_NET_WM_HANDLED_ICONS) \
    X(_NET_WM_USER_TIME) \
    X(_NET_WM_USER_TIME_WINDOW) \
    X(_NET_FRAME_EXTENTS) \
    X(_NET_WM_PING) \
    X(_NET_WM_SYNC_REQUEST) \
    X(_NET_WM_SYNC_REQUEST_COUNTER) \
    X(_NET_WM_FULLSCREEN_MONITORS) \
    X(_NET_WM_FULL_PLACEMENT) \
    X(UTF8_STRING) \
    X(WM_PROTOCOLS) \
    X(MANAGER) \
    X(_NET_WM_WINDOW_TYPE_DESKTOP) \
    X(_NET_WM_WINDOW_TYPE_DOCK) \
    X(_NET_WM_WINDOW_TYPE_TOOLBAR) \
    X(_NET_WM_WINDOW_TYPE_MENU) \
    X(_NET_WM_WINDOW_TYPE_UTILITY) \
    X(_NET_WM_WINDOW_TYPE_SPLASH) \
    X(_NET_WM_WINDOW_TYPE_DIALOG) \
    X(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU) \
    X(_NET_WM_WINDOW_TYPE_POPUP_MENU) \
    X(_NET_WM_WINDOW_TYPE_TOOLTIP) \
    X(_NET_WM_WINDOW_TYPE_NOTIFICATION) \
    X(_NET_WM_WINDOW_TYPE_COMBO) \
    X(_NET_WM_WINDOW_TYPE_DND) \
    X(_NET_WM_WINDOW_TYPE_NORMAL) \
    X(_NET_WM_STATE_MODAL) \
    X(_NET_WM_STATE_STICKY) \
    X(_NET_WM_STATE_MAXIMIZED_VERT) \
    X(_NET_WM_STATE_MAXIMIZED_HORZ) \
    X(_NET_WM_STATE_SHADED) \
    X(_NET_WM_STATE_SKIP_TASKBAR) \
    X(_NET_WM_STATE_SKIP_PAGER) \
    X(_NET_WM_STATE_HIDDEN) \
    X(_NET_WM_STATE_FULLSCREEN) \
    X(_NET_WM_STATE_ABOVE) \
    X(_NET_WM_STATE_BELOW) \
    X(_NET_WM_STATE_DEMANDS_ATTENTION) \
    X(_NET_WM_STATE_FOCUSED) \
    X(_NET_WM_ACTION_MOVE) \
    X(_NET_WM_ACTION_RESIZE) \
    X(_NET_WM_ACTION_MINIMIZE) \
    X(_NET_WM_ACTION_SHADE) \
    X(_NET_WM_ACTION_STICK) \
    X(_NET_WM_ACTION_MAXIMIZE_HORZ) \
    X(_NET_WM_ACTION_MAXIMIZE_VERT) \
    X(_NET_WM_ACTION_FULLSCREEN) \
    X(_NET_WM_ACTION_CHANGE_DESKTOP) \
    X(_NET_WM_ACTION_CLOSE) \
    X(_NET_WM_ACTION_ABOVE) \
    X(_NET_WM_ACTION_BELOW) \
    X(WM_DELETE_WINDOW)

/* get the atom identifier from an atom constant */
#define ATOM(id) (x_atoms[id].atom)

/* constant list expansion of all atoms */
enum {
#define X(atom) atom,
    DEFINE_ALL_ATOMS
#undef X
    ATOM_MAX
};

/* all x atoms, initially all identifiers are 0 until `x_init()` is called. */
extern struct x_atoms {
    /* name of the atom */
    const char *name;
    /* atom identifier */
    xcb_atom_t atom;
} x_atoms[ATOM_MAX];

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

/* cache of window properties */
typedef struct x_properties {
    /* the X window that has these properties */
    xcb_window_t window;

    /** internal porperties */
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
    /* the types of the window in order of importance */
    xcb_atom_t *types;
    /* the pid of the window or 0 if none given */
    uint32_t process_id;
    /* the protocols the window supports */
    xcb_atom_t *protocols;
    /* the region the window should appear at as fullscreen window */
    Extents fullscreen_monitor;

    /** external properties **/
    /* the current desktop of the window (always 0) */
    uint32_t desktop;
    /* border sizes around the window (all always 0) */
    Extents frame_extents;

    /** dual properties (both internal and external) **/
    /* the states of the window */
    xcb_atom_t *states;
} XProperties;

/* connection to the xcb server */
extern xcb_connection_t *connection;

/* general purpose values for xcb function calls */
extern uint32_t general_values[7];

/* the selected X screen */
extern xcb_screen_t *x_screen;

/* Checks if given strut has any reserved space. */
static inline bool is_strut_empty(wm_strut_partial_t *strut)
{
    return strut->reserved.left == 0 && strut->reserved.top == 0 &&
        strut->reserved.right == 0 && strut->reserved.bottom == 0;
}

/* Initialize the X connection and the X atoms. */
int x_initialize(void);

/* Initialize all properties within @properties. */
void x_init_properties(XProperties *properties, xcb_window_t window);

/* Update the property with @properties corresponding to given atom. */
bool x_cache_window_property(XProperties *properties, xcb_atom_t atom);

/* Check if @properties includes @window_type. */
bool x_is_window_type(XProperties *properties, xcb_atom_t window_type);

/* Check if @properties includes @protocol. */
bool x_supports_protocol(XProperties *properties, xcb_atom_t protocol);

/* Check if @properties includes @window_type. */
bool x_is_state(XProperties *properties, xcb_atom_t state);

#endif
