#ifndef ROOT_PROPERTIES_H
#define ROOT_PROPERTIES_H

/* properties set on the root window */
typedef enum root_property {
    /* _NET_SUPPORTED (supported atoms) */
    ROOT_PROPERTY_SUPPORTED,
    /* _NET_CLIENT_LIST / _NET_CLINET_LIST_STACKING (list of managed windows) */
    ROOT_PROPERTY_CLIENT_LIST,
    /* _NET_SUPPORTED (number of desktops) */
    ROOT_PROPERTY_NUMBER_OF_DESKTOPS,
    /* _NET_SUPPORTED (size of the desktop) */
    ROOT_PROPERTY_DESKTOP_GEOMETRY,
    /* _NET_DESKTOP_VIEWPORT (origin of the desktop) */
    ROOT_PROPERTY_DESKTOP_VIEWPORT,
    /* _NET_CURRENT_DESKTOP (current desktop number, 0 indexed) */
    ROOT_PROPERTY_CURRENT_DESKTOP,
    /* _NET_DESKTOP_NAMES (names of each desktop) */
    ROOT_PROPERTY_DESKTOP_NAMES,
    /* _NET_ACTIVE_WINDOW (focused window) */
    ROOT_PROPERTY_ACTIVE_WINDOW,
    /* _NET_WORK_AREA (screen size minus struts) */
    ROOT_PROPERTY_WORK_AREA,
    /* _NET_SUPPORTING_WM_CHECK (child window of root signalling ewmh
     * conformance)
     */
    ROOT_PROPERTY_SUPPORTING_WM_CHECK,

    ROOT_PROPERTY_MAX,
} root_property_t;

/* Synchronize a root property with the X property. */
void synchronize_root_property(root_property_t property);

/* Synchronize all root properties. */
void synchronize_all_root_properties(void);

#endif
