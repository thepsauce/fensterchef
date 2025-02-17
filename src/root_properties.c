#include <string.h>

#include "fensterchef.h"
#include "log.h"
#include "root_properties.h"
#include "window.h"

/* Set the root property _NET_SUPPORTED atom list to the supported atoms. */
static void synchronize_supported(void)
{
    /* set the supported atoms of our window manager, this list was created and
     * understood with the help of:
     * https://specifications.freedesktop.org/wm-spec/latest/index.html#id-1.2
     */
    const xcb_atom_t supported_atoms[] = {
        ATOM(_NET_SUPPORTED),

        ATOM(_NET_CLIENT_LIST),
        ATOM(_NET_CLIENT_LIST_STACKING),

        ATOM(_NET_NUMBER_OF_DESKTOPS),

        ATOM(_NET_DESKTOP_GEOMETRY),
        ATOM(_NET_DESKTOP_VIEWPORT),

        ATOM(_NET_CURRENT_DESKTOP),
        ATOM(_NET_DESKTOP_NAMES),

        ATOM(_NET_ACTIVE_WINDOW),

        ATOM(_NET_WORKAREA),

        ATOM(_NET_SUPPORTING_WM_CHECK),

        ATOM(_NET_SHOWING_DESKTOP),

        ATOM(_NET_CLOSE_WINDOW),

        ATOM(_NET_MOVERESIZE_WINDOW),

        ATOM(_NET_WM_MOVERESIZE),

        ATOM(_NET_RESTACK_WINDOW),

        ATOM(_NET_REQUEST_FRAME_EXTENTS),

        ATOM(_NET_WM_NAME),

        ATOM(_NET_WM_DESKTOP),

        ATOM(_NET_WM_WINDOW_TYPE),
        ATOM(_NET_WM_WINDOW_TYPE_DESKTOP),
        ATOM(_NET_WM_WINDOW_TYPE_DOCK),
        ATOM(_NET_WM_WINDOW_TYPE_TOOLBAR),
        ATOM(_NET_WM_WINDOW_TYPE_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_UTILITY),
        ATOM(_NET_WM_WINDOW_TYPE_SPLASH),
        ATOM(_NET_WM_WINDOW_TYPE_DIALOG),
        ATOM(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_POPUP_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_TOOLTIP),
        ATOM(_NET_WM_WINDOW_TYPE_NOTIFICATION),
        ATOM(_NET_WM_WINDOW_TYPE_COMBO),
        ATOM(_NET_WM_WINDOW_TYPE_DND),
        ATOM(_NET_WM_WINDOW_TYPE_NORMAL),

        ATOM(_NET_WM_STATE),
        ATOM(_NET_WM_STATE_MODAL),
        ATOM(_NET_WM_STATE_STICKY),
        ATOM(_NET_WM_STATE_MAXIMIZED_VERT),
        ATOM(_NET_WM_STATE_MAXIMIZED_HORZ),
        ATOM(_NET_WM_STATE_SHADED),
        ATOM(_NET_WM_STATE_SKIP_TASKBAR),
        ATOM(_NET_WM_STATE_SKIP_PAGER),
        ATOM(_NET_WM_STATE_HIDDEN),
        ATOM(_NET_WM_STATE_FULLSCREEN),
        ATOM(_NET_WM_STATE_ABOVE),
        ATOM(_NET_WM_STATE_BELOW),
        ATOM(_NET_WM_STATE_DEMANDS_ATTENTION),
        ATOM(_NET_WM_STATE_FOCUSED),

        ATOM(_NET_WM_STRUT),
        ATOM(_NET_WM_STRUT_PARTIAL),

        ATOM(_NET_FRAME_EXTENTS),

        ATOM(_NET_WM_FULLSCREEN_MONITORS),

        XCB_ATOM_WM_TRANSIENT_FOR,
    };

    /* set all atoms our window manager supports */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_SUPPORTED), XCB_ATOM_ATOM, 8 * sizeof(uint32_t),
            SIZE(supported_atoms), supported_atoms);
}

/* Set the root property _NET_WORK_AREA to the usable area (screen size minus
 * strut).
 */
static void synchronize_work_area(void)
{
    uint32_t left, top, right, bottom;
    Rectangle rectangle;

    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
    for (Window *window = first_window; window != NULL;
            window = window->next) {
        left += window->properties.strut.reserved.left;
        top += window->properties.strut.reserved.top;
        right += window->properties.strut.reserved.right;
        bottom += window->properties.strut.reserved.bottom;
    }
    rectangle.x = left;
    rectangle.y = top;
    rectangle.width = screen->width_in_pixels - right - left;
    rectangle.height = screen->height_in_pixels - bottom - top;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_WORKAREA), XCB_ATOM_CARDINAL, 8 * sizeof(uint32_t),
            sizeof(rectangle) / sizeof(uint32_t), &rectangle);
}

/* Synchronize a root property with the X property. */
void synchronize_root_property(root_property_t property)
{
    static const char *desktop_name = "0: only desktop";

    const uint32_t zero = 0;
    const uint32_t one = 1;
    const Position origin = { 0, 0 };

    Size geometry;

    switch (property) {
    /* the supported options of our window manager */
    case ROOT_PROPERTY_SUPPORTED:
        synchronize_supported();
        break;

    /* the number of desktops, just set it to 1 */
    case ROOT_PROPERTY_NUMBER_OF_DESKTOPS:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_NUMBER_OF_DESKTOPS), XCB_ATOM_CARDINAL, 32, 1, &one);
        break;

    /* the size of a desktop, we do not support large desktops */
    case ROOT_PROPERTY_DESKTOP_GEOMETRY:
        geometry.width = screen->width_in_pixels;
        geometry.height = screen->height_in_pixels;
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_DESKTOP_GEOMETRY), XCB_ATOM_CARDINAL, 32, 2,
                &geometry);
        break;

    /* viewport of each desktop, just set it to (0, 0) for the only desktop */
    case ROOT_PROPERTY_DESKTOP_VIEWPORT:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_DESKTOP_VIEWPORT), XCB_ATOM_CARDINAL, 32, 2,
                &origin);
        break;

    /* the currently selected desktop, always 0 */
    case ROOT_PROPERTY_CURRENT_DESKTOP:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_CURRENT_DESKTOP), XCB_ATOM_CARDINAL, 32, 1, &zero);
        break;

    /* the name of each desktop */
    case ROOT_PROPERTY_DESKTOP_NAMES:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_DESKTOP_NAMES), ATOM(UTF8_STRING), 8,
                strlen(desktop_name), desktop_name);
        break;

    /* the currently active window */
    case ROOT_PROPERTY_ACTIVE_WINDOW:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                ATOM(_NET_ACTIVE_WINDOW), XCB_ATOM_WINDOW, 32, 1,
                focus_window == NULL ?  &screen->root :
                &focus_window->properties.window);
        break;

    /* the work area (screen space minus strut) */
    case ROOT_PROPERTY_WORK_AREA:
        synchronize_work_area();
        break;

    /* the wm check window is used to identify an ewmh conforming window
     * mangager
     */
    case ROOT_PROPERTY_SUPPORTING_WM_CHECK:
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                screen->root, ATOM(_NET_SUPPORTING_WM_CHECK), XCB_ATOM_WINDOW,
                32, 1, &check_window);
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
