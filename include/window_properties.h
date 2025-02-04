#ifndef WINDOW_PROPERTIES_H
#define WINDOW_PROPERTIES_H

#include <fontconfig/fontconfig.h> // FcChar8

#include <xcb/xcb_icccm.h> // xcb_size_hints_t, xcb_icccm_wm_hints_t,
                           // xcb_ewmh_wm_struct_partial_t

/* these correspond to X atoms */
typedef enum window_property {
    /* WM_NAME */
    WINDOW_PROPERTY_NAME,
    /* WM_NORMAL_HINTS */
    WINDOW_PROPERTY_SIZE_HINTS,
    /* WM_HINTS */
    WINDOW_PROPERTY_HINTS,
    /* WM_STRUT / WM_STRUT_PARTIAL */
    WINDOW_PROPERTY_STRUT,
    /* WM_STATE -> list of atoms that may contain WM_STATE_FULLSCREEN */
    WINDOW_PROPERTY_FULLSCREEN,
    /* WM_TRANSIENT_FOR */
    WINDOW_PROPERTY_TRANSIENT_FOR,

    /* maximum possible value for a window property */
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
    /* the window this window is transient for */
    xcb_window_t transient_for;
    /* if the window is a fullscreen window */
    unsigned is_fullscreen : 1;
} WindowProperties;

/* Checks if given window has any struts set. */
static inline unsigned is_strut_empty(xcb_ewmh_wm_strut_partial_t *struts)
{
    return struts->left == 0 && struts->top == 0 &&
        struts->right == 0 && struts->bottom == 0;
}

/* Update the given property of given window. */
void update_window_property(Window *window, window_property_t property);

/* Update all properties of given window. */
void update_all_window_properties(Window *window);

#endif
