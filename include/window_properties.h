#ifndef WINDOW_PROPERTIES_H
#define WINDOW_PROPERTIES_H

#include <X11/Xatom.h>

#include "window_state.h"

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
    /* the state of the window */ \
    /* ATOM ICON */ X(WM_STATE) \
    /* the name of the locale of the window, fox example: "en_US.UTF-8" */ \
    X(WM_LOCALE_NAME) \
    /* take focus window message atom */ \
    X(WM_TAKE_FOCUS) \
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
    /* the window wants attention */ \
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
    X(_MOTIF_WM_HINTS) \
\
    /* our special property for running commands */ \
    X(FENSTERCHEF_COMMAND)

/* get the atom identifier from an atom constant */
#define ATOM(id) (x_atom_ids[id])

/* constant list expansion of all atoms */
enum {
#define X(atom) atom,
    DEFINE_ALL_ATOMS
#undef X
    ATOM_MAX
};

/* all X atom names */
extern const char *x_atom_names[ATOM_MAX];

/* all X atom ids */
extern Atom x_atom_ids[ATOM_MAX];

/* Intern all X atoms we require. */
void initialize_atoms(void);

/* Set the initial root window properties. */
void initialize_root_properties(void);

/* Gets the `FENSTERCHEF_COMMAND` property from @window. */
char *get_fensterchef_command_property(Window window);

/* Initialize all properties within @window.
 *
 * @mode is set to the mode the window should be in initially.
 *
 * @return any association found that relates to the window.
 */
const struct configuration_association *initialize_window_properties(
        FcWindow *window, window_mode_t *mode);

/* Update the property within @window corresponding to given @atom. */
bool cache_window_property(FcWindow *window, Atom atom);

/* Check if @window supports @protocol. */
bool supports_protocol(FcWindow *window, Atom protocol);

/* Check if @window includes @state. */
bool has_state(FcWindow *window, Atom state);

#endif
