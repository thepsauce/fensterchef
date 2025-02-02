#ifndef WINDOW_PROPERTIES_H
#define WINDOW_PROPERTIES_H

#include <fontconfig/fontconfig.h> // FcChar8

#include <xcb/xcb_icccm.h> // xcb_size_hints_t, xcb_icccm_wm_hints_t,
                           // xcb_ewmh_wm_struct_partial_t

typedef enum window_property {
    WINDOW_PROPERTY_NAME,
    WINDOW_PROPERTY_SIZE_HINTS,
    WINDOW_PROPERTY_HINTS,
    WINDOW_PROPERTY_STRUT,
    WINDOW_PROPERTY_FULLSCREEN,
    WINDOW_PROPERTY_TRANSIENT,

    WINDOW_PROPERTY_MAX,
} window_property_t;

/* forward declaration */
struct window;
typedef struct window Window;

/* Properties are additional fields that applications set to describe a window.
 * These properties are set up to date using the xcb atom properties.
 */
typedef struct window_properties {
    /* short window name */
    FcChar8 short_name[256];
    /* xcb size hints of the window */
    xcb_size_hints_t size_hints;
    /* special window manager hints */
    xcb_icccm_wm_hints_t hints;
    /* window struts (extents) */
    xcb_ewmh_wm_strut_partial_t struts;
    /* if the window has the strut partial property */
    unsigned has_strut : 1;
    /* if the window has the strut partial property */
    unsigned has_strut_partial : 1;
    /* if the window is a fullscreen window */
    unsigned is_fullscreen : 1;
    /* if the window is a transient window */
    unsigned is_transient : 1;
} WindowProperties;

/* Update the given property of given window. */
void update_window_property(Window *window, window_property_t property);

/* Update all properties of given window. */
void update_all_window_properties(Window *window);

#endif
