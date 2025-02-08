#ifndef WINDOW_PROPERTIES_H
#define WINDOW_PROPERTIES_H

#include <fontconfig/fontconfig.h> // FcChar8

#include <xcb/xcb_icccm.h> // xcb_size_hints_t, xcb_icccm_wm_hints_t,
                           // xcb_ewmh_wm_struct_partial_t

/* these correspond to X atoms set by the client */
typedef enum window_property {
    /* _NET_WM_NAME */
    WINDOW_PROPERTY_NAME,
    /* _NET_WM_NORMAL_HINTS / _NET_WM_SIZE_HINTS */
    WINDOW_PROPERTY_SIZE_HINTS,
    /* _NET_WM_HINTS */
    WINDOW_PROPERTY_HINTS,
    /* _NET_WM_STRUT / _NET_WM_STRUT_PARTIAL */
    WINDOW_PROPERTY_STRUT,
    /* _NET_WM_WINDOW_TYPE */
    WINDOW_PROPERTY_WINDOW_TYPE,
    /* _NET_WM_STATE */
    WINDOW_PROPERTY_STATE,
    /* _NET_WM_TRANSIENT_FOR */
    WINDOW_PROPERTY_TRANSIENT_FOR,
    /* _NET_WM_PID */
    WINDOW_PROPERTY_PID,
    /* WM_PROTOCOLS */
    WINDOW_PROPERTY_PROTOCOLS,
    /* _NET_WM_FULLSCREEN_MONITORS */
    WINDOW_PROPERTY_FULLSCREEN_MONITORS,

    /* maximum possible value for a window property */
    WINDOW_PROPERTY_MAX,
} window_property_t;

/* these correspond to X atoms we set for the windows */
typedef enum window_external_property {
    /* _NET_DESKTOP */
    WINDOW_EXTERNAL_PROPERTY_DESKTOP,
    /* _NET_FRAME_EXTENTS */
    WINDOW_EXTERNAL_PROPERTY_FRAME_EXTENTS,
    /* _NET_WM_STATE */
    WINDOW_EXTERNAL_PROPERTY_STATE,
    /* _NET_WM_ALLOWED_ACTIONS */
    WINDOW_EXTERNAL_PROPERTY_ALLOWED_ACTIONS,

    /* maximum possible value for an external window property */
    WINDOW_EXTERNAL_PROPERTY_MAX,
} window_external_property_t;

/* forward declaration */
struct window;
typedef struct window Window;

/* Properties are additional fields that applications set to describe a window.
 * These properties are set up to date using the xcb atom properties.
 */
typedef struct window_properties {
    /** internal porperties */
    /* window name */
    FcChar8 *name;
    /* xcb size hints of the window */
    xcb_size_hints_t size_hints;
    /* special window manager hints */
    xcb_icccm_wm_hints_t hints;
    /* window struts (extents) */
    xcb_ewmh_wm_strut_partial_t struts;
    /* the window this window is transient for */
    xcb_window_t transient_for;
    /* the types of the window in order of importance */
    xcb_atom_t *types;
    uint32_t number_of_types;
    /* the pid of the window or 0 if none given */
    uint32_t process_id;
    /* the protocols the window supports */
    xcb_atom_t *protocols;
    uint32_t number_of_protocols;
    /* the rect the window should appear at as fullscreen window */
    xcb_ewmh_get_wm_fullscreen_monitors_reply_t fullscreen_monitor;

    /** external properties **/
    /* the current desktop of the window (always 0) */
    uint32_t desktop;
    /* border sizes around the window (all always 0) */
    xcb_ewmh_get_extents_reply_t frame_extents;

    /** dual properties (both internal and external) **/
    /* the states of the window */
    xcb_atom_t *states;
    uint32_t number_of_states;
} WindowProperties;

/* Checks if given window has any struts set. */
static inline bool is_strut_empty(xcb_ewmh_wm_strut_partial_t *struts)
{
    return struts->left == 0 && struts->top == 0 &&
        struts->right == 0 && struts->bottom == 0;
}

/* Check if an atom is within the given list of atoms. */
bool is_atom_included(const xcb_atom_t *atoms, uint32_t number_of_atoms,
        xcb_atom_t atom);

/* Check if a window has a specific window type. */
#define does_window_have_type(window, type) \
    is_atom_included(window->properties.types, \
            window->properties.number_of_types, type)

/* Check if a window has a specific state. */
#define does_window_have_state(window, state) \
    is_atom_included(window->properties.states, \
            window->properties.number_of_states, state)

/* Check if a window has a specific protocol. */
#define does_window_have_protocol(window, protocol) \
    is_atom_included(window->properties.protocols, \
            window->properties.number_of_protocols, protocol)

/* Update the given property of given window. */
void update_window_property(Window *window, window_property_t property);

/* Update all properties of given window. */
void update_all_window_properties(Window *window);

/* Update the property of given window corresponding to given atom. */
bool update_window_property_by_atom(Window *window, xcb_atom_t atom);

/* Synchronize the given external property of given window. */
void synchronize_window_property(Window *window,
        window_external_property_t property);

/* Synchronize all external properties of given window. */
void synchronize_all_window_properties(Window *window);

#endif
