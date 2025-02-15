#include <inttypes.h>
#include <string.h>

#include "fensterchef.h"
#include "log.h"
#include "window.h"
#include "root_properties.h"
#include "screen.h"

/* Checks if an atom is within the given list of atoms. */
bool is_atom_included(const xcb_atom_t *atoms, uint32_t number_of_atoms,
        xcb_atom_t atom)
{
    for (uint32_t i = 0; i < number_of_atoms; i++) {
        if (atoms[i] == atom) {
            return true;
        }
    }
    return false;
}

/* Update the short_name of given window. */
static void update_window_name(Window *window)
{
    xcb_get_property_cookie_t name_cookie;
    xcb_ewmh_get_utf8_strings_reply_t data;
    xcb_get_property_reply_t *name;
    xcb_generic_error_t *error;

    name_cookie = xcb_ewmh_get_wm_name(&ewmh, window->xcb_window);

    free(window->properties.name);

    if (!xcb_ewmh_get_wm_name_reply(&ewmh, name_cookie, &data, NULL)) {
        /* fall back to WM_NAME */
        name_cookie = xcb_get_property(connection, 0, window->xcb_window,
                XCB_ATOM_WM_NAME, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
        name = xcb_get_property_reply(connection, name_cookie, &error);
        if (name == NULL) {
            LOG_ERROR(error, "could not get window name");
            window->properties.name = NULL;
            return;
        }
        window->properties.name = (uint8_t*) xstrndup(
                xcb_get_property_value(name),
                xcb_get_property_value_length(name));
        free(name);
        return;
    }

    window->properties.name = (uint8_t*) xstrndup(data.strings,
            data.strings_len);

    xcb_ewmh_get_utf8_strings_reply_wipe(&data);
}

/* Update the size_hints of given window. */
static void update_window_size_hints(Window *window)
{
    xcb_get_property_cookie_t size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(connection, window->xcb_window,
            XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(connection, size_hints_cookie,
                &window->properties.size_hints, NULL)) {
        window->properties.size_hints.flags = 0;
    }
}

/* Update the hints of given window. */
static void update_window_hints(Window *window)
{
    xcb_get_property_cookie_t hints_cookie;

    hints_cookie = xcb_icccm_get_wm_hints(connection, window->xcb_window);
    if (!xcb_icccm_get_wm_hints_reply(connection, hints_cookie,
                &window->properties.hints, NULL)) {
        window->properties.hints.flags = 0;
    }
}

/* Update the strut partial property of given window. */
static void update_window_strut(Window *window)
{
    xcb_ewmh_wm_strut_partial_t new_struts;
    xcb_get_property_cookie_t strut_partial_cookie;
    xcb_get_property_cookie_t strut_cookie;
    xcb_ewmh_get_extents_reply_t struts;

    memset(&new_struts, 0, sizeof(new_struts));

    strut_partial_cookie = xcb_ewmh_get_wm_strut_partial(&ewmh,
            window->xcb_window);
    if (!xcb_ewmh_get_wm_strut_partial_reply(&ewmh, strut_partial_cookie,
                &new_struts, NULL)) {
        /* _NET_WM_STRUT is older than _NET_WM_STRUT_PARTIAL, fall back to it when
         * there is no strut partial
         */
        strut_cookie = xcb_ewmh_get_wm_strut(&ewmh, window->xcb_window);
        if (xcb_ewmh_get_wm_strut_reply(&ewmh, strut_cookie, &struts, NULL)) {
            new_struts.left = struts.left;
            new_struts.top = struts.top;
            new_struts.right = struts.right;
            new_struts.bottom = struts.bottom;
        }
    }

    /* check if the struts have changed */
    if (memcmp(&window->properties.struts, &new_struts, sizeof(new_struts)) != 0) {
        window->properties.struts = new_struts;
        if (window->state.is_visible) {
            reconfigure_monitor_frame_sizes();
            synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
        }
    }
}

/* Update the types property of given window. */
static void update_window_type(Window *window)
{
    xcb_get_property_cookie_t type_cookie;
    xcb_ewmh_get_atoms_reply_t type;

    type_cookie = xcb_ewmh_get_wm_window_type(&ewmh, window->xcb_window);
    if (!xcb_ewmh_get_wm_window_type_reply(&ewmh, type_cookie, &type,
                NULL)) {
        window->properties.types = NULL;
        window->properties.number_of_types = 0;
        return;
    }

    window->properties.types = xmemdup(type.atoms,
            sizeof(*type.atoms) * type.atoms_len);
    window->properties.number_of_types = type.atoms_len;

    if (does_window_have_type(window, ewmh._NET_WM_WINDOW_TYPE_DOCK)) {
        general_values[0] = 0;
        xcb_configure_window(connection, window->xcb_window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
    }

    xcb_ewmh_get_atoms_reply_wipe(&type);
}

/* Update the states property of given window. */
static void update_window_state(Window *window)
{
    xcb_get_property_cookie_t state_cookie;
    xcb_ewmh_get_atoms_reply_t state;

    state_cookie = xcb_ewmh_get_wm_state(&ewmh, window->xcb_window);
    if (!xcb_ewmh_get_wm_state_reply(&ewmh, state_cookie, &state,
                NULL)) {
        window->properties.states = NULL;
        window->properties.number_of_states = 0;
        return;
    }

    window->properties.states = xmemdup(state.atoms,
            sizeof(*state.atoms) * state.atoms_len);
    window->properties.number_of_states = state.atoms_len;

    xcb_ewmh_get_atoms_reply_wipe(&state);
}

/* Update the transient_for property of given window. */
static void update_window_transient_for(Window *window)
{
    xcb_get_property_cookie_t transient_for_cookie;

    transient_for_cookie = xcb_icccm_get_wm_transient_for(connection,
            window->xcb_window);
    if (!xcb_icccm_get_wm_transient_for_reply(connection, transient_for_cookie,
                &window->properties.transient_for, NULL)) {
        window->properties.transient_for = 0;
    }
}

/* Update the process_id property of given window. */
static void update_window_pid(Window *window)
{
    xcb_get_property_cookie_t pid_cookie;

    pid_cookie = xcb_ewmh_get_wm_pid(&ewmh, window->xcb_window);
    if (!xcb_ewmh_get_wm_pid_reply(&ewmh, pid_cookie,
                &window->properties.process_id, NULL)) {
        window->properties.process_id = 0;
    }
}

/* Update the protocols property of given window. */
static void update_window_protocols(Window *window)
{
    xcb_get_property_cookie_t protocols_cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;

    protocols_cookie = xcb_icccm_get_wm_protocols(connection, window->xcb_window,
            ewmh.WM_PROTOCOLS);
    if (!xcb_icccm_get_wm_protocols_reply(connection, protocols_cookie, &protocols,
                NULL)) {
        window->properties.protocols = NULL;
        window->properties.number_of_protocols = 0;
        return;
    }

    window->properties.protocols = xmemdup(protocols.atoms,
            sizeof(*protocols.atoms) * protocols.atoms_len);
    window->properties.number_of_protocols = protocols.atoms_len;

    xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
}

/* Update the fullscreen monitors property of given window. */
static void update_window_fullscreen_monitors(Window *window)
{
    xcb_get_property_cookie_t monitors_cookie;

    monitors_cookie = xcb_ewmh_get_wm_fullscreen_monitors(&ewmh,
            window->xcb_window);
    if (!xcb_ewmh_get_wm_fullscreen_monitors_reply(&ewmh, monitors_cookie,
                &window->properties.fullscreen_monitor, NULL)) {
        memset(&window->properties.fullscreen_monitor, 0,
                sizeof(window->properties.fullscreen_monitor));
    }
}

/* Update the given property of given window. */
void update_window_property(Window *window, window_property_t property)
{
    static void (*const updaters[])(Window *window) = {
        [WINDOW_PROPERTY_NAME] = update_window_name,
        [WINDOW_PROPERTY_SIZE_HINTS] = update_window_size_hints,
        [WINDOW_PROPERTY_HINTS] = update_window_hints,
        [WINDOW_PROPERTY_STRUT] = update_window_strut,
        [WINDOW_PROPERTY_WINDOW_TYPE] = update_window_type,
        [WINDOW_PROPERTY_STATE] = update_window_state,
        [WINDOW_PROPERTY_TRANSIENT_FOR] = update_window_transient_for,
        [WINDOW_PROPERTY_PID] = update_window_pid,
        [WINDOW_PROPERTY_PROTOCOLS] = update_window_protocols,
        [WINDOW_PROPERTY_FULLSCREEN_MONITORS] =
            update_window_fullscreen_monitors,
    };

    updaters[property](window);
}

/* Update all properties of given window. */
void update_all_window_properties(Window *window)
{
    for (window_property_t i = 0; i < WINDOW_PROPERTY_MAX; i++) {
        update_window_property(window, i);
    }
}

/* Update the property of given window corresponding to given atom. */
bool update_window_property_by_atom(Window *window, xcb_atom_t atom)
{
    if (atom == XCB_ATOM_WM_NAME || atom == ewmh._NET_WM_NAME) {
        update_window_property(window, WINDOW_PROPERTY_NAME);
    } else if (atom == XCB_ATOM_WM_NORMAL_HINTS ||
            atom == XCB_ATOM_WM_SIZE_HINTS) {
        update_window_property(window, WINDOW_PROPERTY_SIZE_HINTS);
    } else if (atom == XCB_ATOM_WM_HINTS) {
        update_window_property(window, WINDOW_PROPERTY_HINTS);
    } else if (atom == ewmh._NET_WM_STRUT ||
            atom == ewmh._NET_WM_STRUT_PARTIAL) {
        update_window_property(window, WINDOW_PROPERTY_STRUT);
    } else if (atom == ewmh._NET_WM_WINDOW_TYPE) {
        update_window_property(window, WINDOW_PROPERTY_WINDOW_TYPE);
    } else if (atom == ewmh._NET_WM_STATE) {
        update_window_property(window, WINDOW_PROPERTY_STATE);
    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {
        update_window_property(window, WINDOW_PROPERTY_TRANSIENT_FOR);
    } else if (atom == ewmh._NET_WM_PID) {
        update_window_property(window, WINDOW_PROPERTY_PID);
    } else if (atom == ewmh.WM_PROTOCOLS) {
        update_window_property(window, WINDOW_PROPERTY_PROTOCOLS);
    } else if (atom == ewmh._NET_WM_FULLSCREEN_MONITORS) {
        update_window_property(window, WINDOW_PROPERTY_FULLSCREEN_MONITORS);
    } else {
        return false;
    }
    return true;
}

/* Checks if the window has a specific window type. */
bool window_has_type(Window *window, xcb_atom_t window_type)
{
    for (uint32_t i = 0; i < window->properties.number_of_types; i++) {
        if (window->properties.types[i] == window_type) {
            return true;
        }
    }
    return false;
}

/* Synchronize the desktop atom with given window's desktop. */
void synchronize_desktop(Window *window)
{
    xcb_ewmh_set_wm_desktop(&ewmh, window->xcb_window,
            window->properties.desktop);
}

/* Synchronize the frame exents atom with given window's frame extents. */
void synchronize_frame_extents(Window *window)
{
    xcb_ewmh_set_frame_extents(&ewmh, window->xcb_window,
            window->properties.frame_extents.left,
            window->properties.frame_extents.top,
            window->properties.frame_extents.right,
            window->properties.frame_extents.bottom);
}

/* Synchronize the frame exents atom with given window's frame extents. */
void synchronize_property_state(Window *window)
{
    (void) window;
}

/* Synchronize current atom with given window's allowed actions. */
void synchronize_allowed_actions(Window *window)
{
    const xcb_atom_t atom_lists[WINDOW_MODE_MAX][16] = {
        [WINDOW_MODE_TILING] = {
            ewmh._NET_WM_ACTION_MAXIMIZE_HORZ,
            ewmh._NET_WM_ACTION_MAXIMIZE_VERT,
            ewmh._NET_WM_ACTION_FULLSCREEN,
            ewmh._NET_WM_ACTION_CHANGE_DESKTOP,
            ewmh._NET_WM_ACTION_CLOSE,
            XCB_NONE,
        },

        [WINDOW_MODE_POPUP] = {
            ewmh._NET_WM_ACTION_MOVE,
            ewmh._NET_WM_ACTION_RESIZE,
            ewmh._NET_WM_ACTION_MINIMIZE,
            ewmh._NET_WM_ACTION_SHADE,
            ewmh._NET_WM_ACTION_STICK,
            ewmh._NET_WM_ACTION_MAXIMIZE_HORZ,
            ewmh._NET_WM_ACTION_MAXIMIZE_VERT,
            ewmh._NET_WM_ACTION_FULLSCREEN,
            ewmh._NET_WM_ACTION_CHANGE_DESKTOP,
            ewmh._NET_WM_ACTION_CLOSE,
            ewmh._NET_WM_ACTION_ABOVE,
            ewmh._NET_WM_ACTION_BELOW,
            XCB_NONE,
        },

        [WINDOW_MODE_FULLSCREEN] = {
            ewmh._NET_WM_ACTION_CHANGE_DESKTOP,
            ewmh._NET_WM_ACTION_CLOSE,
            ewmh._NET_WM_ACTION_ABOVE,
            ewmh._NET_WM_ACTION_BELOW,
            XCB_NONE,
        },

        [WINDOW_MODE_DOCK] = {
            XCB_NONE,
        },
    };
    const xcb_atom_t *list;
    uint32_t list_length;
    xcb_atom_t *duplicate_list;

    list = atom_lists[window->state.mode];
    for (list_length = 0; list_length < SIZE(atom_lists[0]); list_length++) {
        if (list[list_length] == XCB_NONE) {
            break;
        }
    }

    duplicate_list = xmemdup(list, sizeof(*list) * list_length);
    xcb_ewmh_set_wm_allowed_actions(&ewmh, window->xcb_window, list_length,
            duplicate_list);
}

/* Synchronize the given external property of given window. */
void synchronize_window_property(Window *window,
        window_external_property_t property)
{
    static void (*const synchronizers[])(Window *window) = {
        [WINDOW_EXTERNAL_PROPERTY_DESKTOP] = synchronize_desktop,
        [WINDOW_EXTERNAL_PROPERTY_FRAME_EXTENTS] = synchronize_frame_extents,
        [WINDOW_EXTERNAL_PROPERTY_STATE] = synchronize_property_state,
        [WINDOW_EXTERNAL_PROPERTY_ALLOWED_ACTIONS] = synchronize_allowed_actions,
    };

    synchronizers[property](window);
}

/* Synchronize all external properties of given window. */
void synchronize_all_window_properties(Window *window)
{
    for (window_external_property_t i = 0; i < WINDOW_EXTERNAL_PROPERTY_MAX;
            i++) {
        synchronize_window_property(window, i);
    }
}
