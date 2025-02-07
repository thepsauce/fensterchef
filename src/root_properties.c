#include <string.h>

#include "fensterchef.h"
#include "root_properties.h"
#include "screen.h"
#include "window.h"

/* Set the root property _NET_SUPPORTED atom list to the supported atoms. */
static void synchronize_supported(void)
{
    /* set the supported atoms of our window manager, this list was created and
     * understood with the help of:
     * https://specifications.freedesktop.org/wm-spec/latest/index.html#id-1.2
     */
    const xcb_atom_t supported_atoms[] = {
        ewmh._NET_SUPPORTED,

        ewmh._NET_CLIENT_LIST,
        ewmh._NET_CLIENT_LIST_STACKING,

        ewmh._NET_NUMBER_OF_DESKTOPS,

        ewmh._NET_DESKTOP_GEOMETRY,
        ewmh._NET_DESKTOP_VIEWPORT,

        ewmh._NET_CURRENT_DESKTOP,
        ewmh._NET_DESKTOP_NAMES,

        ewmh._NET_ACTIVE_WINDOW,

        ewmh._NET_WORKAREA,

        ewmh._NET_SUPPORTING_WM_CHECK,

        ewmh._NET_SHOWING_DESKTOP,

        ewmh._NET_CLOSE_WINDOW,

        ewmh._NET_MOVERESIZE_WINDOW,

        ewmh._NET_WM_MOVERESIZE,

        ewmh._NET_RESTACK_WINDOW,

        ewmh._NET_REQUEST_FRAME_EXTENTS,

        ewmh._NET_WM_NAME,
        ewmh._NET_WM_VISIBLE_NAME,
        ewmh._NET_WM_ICON_NAME,
        ewmh._NET_WM_VISIBLE_ICON_NAME,

        ewmh._NET_WM_DESKTOP,

        ewmh._NET_WM_WINDOW_TYPE,
        ewmh._NET_WM_WINDOW_TYPE_DESKTOP,
        ewmh._NET_WM_WINDOW_TYPE_DOCK,
        ewmh._NET_WM_WINDOW_TYPE_TOOLBAR,
        ewmh._NET_WM_WINDOW_TYPE_MENU,
        ewmh._NET_WM_WINDOW_TYPE_UTILITY,
        ewmh._NET_WM_WINDOW_TYPE_SPLASH,
        ewmh._NET_WM_WINDOW_TYPE_DIALOG,
        ewmh._NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        ewmh._NET_WM_WINDOW_TYPE_POPUP_MENU,
        ewmh._NET_WM_WINDOW_TYPE_TOOLTIP,
        ewmh._NET_WM_WINDOW_TYPE_NOTIFICATION,
        ewmh._NET_WM_WINDOW_TYPE_COMBO,
        ewmh._NET_WM_WINDOW_TYPE_DND,
        ewmh._NET_WM_WINDOW_TYPE_NORMAL,

        ewmh._NET_WM_STATE,
        ewmh._NET_WM_STATE_MODAL,
        ewmh._NET_WM_STATE_STICKY,
        ewmh._NET_WM_STATE_MAXIMIZED_VERT,
        ewmh._NET_WM_STATE_MAXIMIZED_HORZ,
        ewmh._NET_WM_STATE_SHADED,
        ewmh._NET_WM_STATE_SKIP_TASKBAR,
        ewmh._NET_WM_STATE_SKIP_PAGER,
        ewmh._NET_WM_STATE_HIDDEN,
        ewmh._NET_WM_STATE_FULLSCREEN,
        ewmh._NET_WM_STATE_ABOVE,
        ewmh._NET_WM_STATE_BELOW,
        ewmh._NET_WM_STATE_DEMANDS_ATTENTION,

        ewmh._NET_WM_STRUT,
        ewmh._NET_WM_STRUT_PARTIAL,

        ewmh._NET_FRAME_EXTENTS,

        ewmh._NET_WM_PING,

        ewmh._NET_WM_FULLSCREEN_MONITORS,

        XCB_ATOM_WM_TRANSIENT_FOR,
    };

    xcb_ewmh_set_supported(&ewmh, screen->number, SIZE(supported_atoms),
            /* xcb_ewmh_set_supported() expects a non const value, however, it
             * is just passed into a function expecting a const value
             */
            (xcb_atom_t*) supported_atoms);
}

/* Set the root property _NET_WORK_AREA to the usable area (screen size minus
 * struts).
 */
static void synchronize_work_area(void)
{
    uint32_t left, top, right, bottom;
    xcb_ewmh_geometry_t geometry;

    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
    for (Window *window = first_window; window != NULL;
            window = window->next) {
        left += window->properties.struts.left;
        top += window->properties.struts.top;
        right += window->properties.struts.right;
        bottom += window->properties.struts.bottom;
    }
    geometry.x = left;
    geometry.y = top;
    geometry.width = screen->xcb_screen->width_in_pixels - right - left;
    geometry.height = screen->xcb_screen->height_in_pixels - bottom - top;
    xcb_ewmh_set_workarea(&ewmh, screen->number, 1, &geometry);
}

/* Synchronize a root property with the X property. */
void synchronize_root_property(root_property_t property)
{
    static const char *desktop_name = "0: only desktop";

    xcb_ewmh_coordinates_t coordinate;

    switch (property) {
    /* the supported options of our window manager */
    case ROOT_PROPERTY_SUPPORTED:
        synchronize_supported();
        break;

    /* the number of desktops, just set it to 1 */
    case ROOT_PROPERTY_NUMBER_OF_DESKTOPS:
        xcb_ewmh_set_number_of_desktops(&ewmh, screen->number, 1);
        break;

    /* the size of a desktop, we do not support large desktops */
    case ROOT_PROPERTY_DESKTOP_GEOMETRY:
        xcb_ewmh_set_desktop_geometry(&ewmh, screen->number,
                screen->xcb_screen->width_in_pixels,
                screen->xcb_screen->height_in_pixels);
        break;

    /* viewport of each desktop, just set it to (0, 0) for the only desktop */
    case ROOT_PROPERTY_DESKTOP_VIEWPORT:
        coordinate.x = 0;
        coordinate.y = 0;
        xcb_ewmh_set_desktop_viewport(&ewmh, screen->number, 1,
                &coordinate);
        break;

    /* the currently selected desktop, always 0 */
    case ROOT_PROPERTY_CURRENT_DESKTOP:
        xcb_ewmh_set_current_desktop(&ewmh, screen->number, 0);
        break;

    /* the name of each desktop */
    case ROOT_PROPERTY_DESKTOP_NAMES:
        xcb_ewmh_set_desktop_names(&ewmh, screen->number,
                strlen(desktop_name), desktop_name);
        break;

    /* the currently active window */
    case ROOT_PROPERTY_ACTIVE_WINDOW:
        xcb_ewmh_set_active_window(&ewmh, screen->number, focus_window == NULL ?
                screen->xcb_screen->root : focus_window->xcb_window);
        break;

    /* the work area (screen space minus struts) */
    case ROOT_PROPERTY_WORK_AREA:
        synchronize_work_area();
        break;

    /* the wm check window is used to identify an ewmh conforming window
     * mangager
     */
    case ROOT_PROPERTY_SUPPORTING_WM_CHECK:
        xcb_ewmh_set_supporting_wm_check(&ewmh, screen->xcb_screen->root,
                screen->check_window);
        break;

    /* not a real property */
    case ROOT_PROPERTY_MAX:
        break;
    }
}

/* Synchronize all root properties. */
void synchronize_all_root_properties(void)
{
    for (root_property_t i = 0; i < ROOT_PROPERTY_MAX; i++) {
        synchronize_root_property(i);
    }
}
