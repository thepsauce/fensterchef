#include <string.h>

#include "log.h"
#include "x.h"

/* connection to the xcb server */
xcb_connection_t *connection;

/* the selected x screen */
xcb_screen_t *x_screen;

/* general purpose values for xcb function calls */
uint32_t general_values[7];

struct x_atoms x_atoms[] = {
#define X(atom) { #atom, 0 },
    DEFINE_ALL_ATOMS
#undef X
};

/* Get the screen with given screen number. */
static xcb_screen_t *x_get_screen(int screen_number)
{
    const xcb_setup_t *setup;
    xcb_screen_iterator_t i;
    
    setup = xcb_get_setup(connection);
    for(i = xcb_setup_roots_iterator(setup); i.rem > 0; xcb_screen_next(&i)) {
        if (screen_number == 0) {
            return i.data;
        }
        screen_number--;
    }
    return NULL;
}

/* Initialize the xcb connection and the xcb atoms. */
int x_initialize(void)
{
    int screen_number;
    xcb_intern_atom_cookie_t atom_cookies[ATOM_MAX];
    xcb_generic_error_t *error;
    xcb_intern_atom_reply_t *atom;

    /* read the DISPLAY environment variable to determine the display to
     * attach to; if the DISPLAY variable is in the form :X.Y then X is the
     * display number and Y the screen number which is stored in `screen_number
     */
    connection = xcb_connect(NULL, &screen_number);
    /* standard way to check if a connection failed */
    if (xcb_connection_has_error(connection) > 0) {
        LOG_ERROR(NULL, "could not create xcb connection");
        return ERROR;
    }

    x_screen = x_get_screen(screen_number);
    if (x_screen == NULL) {
        LOG_ERROR(NULL, "there is no screen %d", screen_number);
        return ERROR;
    }

    /* intern the atoms into the xcb server */
    for (uint32_t i = 0; i < ATOM_MAX; i++) {
        atom_cookies[i] = xcb_intern_atom(connection, false,
                strlen(x_atoms[i].name), x_atoms[i].name);
    }

    /* get the reply for all cookies and set the atoms to the values the xcb
     * server assigned for us
     */
    for (uint32_t i = 0; i < ATOM_MAX; i++) {
        atom = xcb_intern_atom_reply(connection, atom_cookies[i], &error);
        if (atom == NULL) {
            LOG_ERROR(error, "could not intern atom %s", x_atoms[i].name);
            return ERROR;
        }
        x_atoms[i].atom = atom->atom;
        free(atom);
    }

    return OK;
}

/* Checks if an atom is within the given list of atoms. */
static inline bool x_is_atom_included(const xcb_atom_t *atoms, xcb_atom_t atom)
{
    for (; atoms[0] != XCB_NONE; atoms++) {
        if (atoms[0] == atom) {
            return true;
        }
    }
    return false;
}

/* Check if @properties includes @window_type. */
bool x_is_window_type(XProperties *properties, xcb_atom_t window_type)
{
    return x_is_atom_included(properties->types, window_type);
}

/* Check if @properties includes @protocol. */
bool x_supports_protocol(XProperties *properties, xcb_atom_t protocol)
{
    return x_is_atom_included(properties->protocols, protocol);
}

/* Check if @properties includes @window_type. */
bool x_is_state(XProperties *properties, xcb_atom_t state)
{
    return x_is_atom_included(properties->states, state);
}

/* Wrapper around getting a cookie and reply for a GetProperty request. */
static inline xcb_get_property_reply_t *x_get_property(xcb_window_t window,
        xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t length,
        xcb_generic_error_t **error)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    cookie = xcb_get_property(connection, false, window, property, type, 0,
            length);
    reply = xcb_get_property_reply(connection, cookie, error);
    if (reply == NULL) {
        return NULL;
    }
    /* check if the property is in the needed format and if it is long enough */
    if (reply->format != format || (length != UINT32_MAX &&
                (uint32_t) xcb_get_property_value_length(reply) <
                length * format / 8)) {
        free(reply);
        return NULL;
    }
    return reply;
}

/* Update the name of given window. */
static void x_update_window_name(XProperties *properties)
{
    xcb_get_property_reply_t *name;

    free(properties->name);

    name = x_get_property(properties->window, ATOM(_NET_WM_NAME),
            XCB_GET_PROPERTY_TYPE_ANY, 8, UINT32_MAX, NULL);
    if (name == NULL) {
        /* fall back to WM_NAME */
        name = x_get_property(properties->window, XCB_ATOM_WM_NAME,
                XCB_GET_PROPERTY_TYPE_ANY, 8, UINT32_MAX, NULL);
        if (name == NULL) {
            properties->name = NULL;
            return;
        }
    }

    properties->name = (uint8_t*) xstrndup(
            xcb_get_property_value(name),
            xcb_get_property_value_length(name));

    free(name);
}

/* Update the size_hints of given window. */
static void x_update_window_size_hints(XProperties *properties)
{
    xcb_get_property_cookie_t size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(connection,
            properties->window, XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(connection, size_hints_cookie,
                &properties->size_hints, NULL)) {
        properties->size_hints.flags = 0;
    }
}

/* Update the hints of given window. */
static void x_update_window_hints(XProperties *properties)
{
    xcb_get_property_cookie_t hints_cookie;

    hints_cookie = xcb_icccm_get_wm_hints(connection, properties->window);
    if (!xcb_icccm_get_wm_hints_reply(connection, hints_cookie,
                &properties->hints, NULL)) {
        properties->hints.flags = 0;
    }
}

/* Update the strut partial property of given window. */
static void x_update_window_strut(XProperties *properties)
{
    wm_strut_partial_t new_strut;
    xcb_get_property_reply_t *strut;

    memset(&new_strut, 0, sizeof(new_strut));

    strut = x_get_property(properties->window,
            ATOM(_NET_WM_STRUT_PARTIAL), XCB_ATOM_CARDINAL, 32,
            sizeof(wm_strut_partial_t) / sizeof(uint32_t), NULL);
    if (strut == NULL) {
        /* _NET_WM_STRUT is older than _NET_WM_STRUT_PARTIAL, fall back to it
         * when there is no strut partial
         */
        strut = x_get_property(properties->window,
                ATOM(_NET_WM_STRUT), XCB_ATOM_CARDINAL, 32,
                sizeof(Extents) / sizeof(uint32_t), NULL);
        if (strut == NULL) {
            return;
        }
        new_strut.reserved = *(Extents*) xcb_get_property_value(strut);
    } else {
        new_strut = *(wm_strut_partial_t*) xcb_get_property_value(strut);
    }

    free(strut);

    properties->strut = new_strut;
}

/* Get a window property as list of atoms. */
static inline xcb_atom_t *x_get_atom_list(xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_atom_t *atoms;

    cookie = xcb_get_property(connection, 0, window, atom, XCB_ATOM_ATOM,
            0, UINT32_MAX);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    if (reply == NULL) {
        return xcalloc(1, sizeof(*atoms));
    }
    atoms = xmalloc(xcb_get_property_value_length(reply) + sizeof(*atoms));
    memcpy(atoms, xcb_get_property_value(reply),
            xcb_get_property_value_length(reply));
    atoms[xcb_get_property_value_length(reply) / sizeof(xcb_atom_t)] = XCB_NONE;
    free(reply);
    return atoms;
}

/* Update the types property of given window. */
static void x_update_window_type(XProperties *properties)
{
    properties->types = x_get_atom_list(properties->window,
            ATOM(_NET_WM_WINDOW_TYPE));
}

/* Update the states property of given window. */
static void x_update_window_state(XProperties *properties)
{
    properties->states = x_get_atom_list(properties->window,
            ATOM(_NET_WM_STATE));
}

/* Update the transient_for property of given window. */
static void x_update_window_transient_for(XProperties *properties)
{
    xcb_get_property_cookie_t transient_for_cookie;

    transient_for_cookie = xcb_icccm_get_wm_transient_for(connection,
            properties->window);
    if (!xcb_icccm_get_wm_transient_for_reply(connection, transient_for_cookie,
                &properties->transient_for, NULL)) {
        properties->transient_for = 0;
    }
}

/* Update the process_id property of given window. */
static void x_update_window_pid(XProperties *properties)
{
    xcb_get_property_reply_t *pid;

    pid = x_get_property(properties->window, ATOM(_NET_WM_PID),
            XCB_ATOM_CARDINAL, 32, 1, NULL);
    if (pid == NULL) {
        properties->process_id = 0;
    } else {
        properties->process_id = *(uint32_t*) xcb_get_property_value(pid);
        free(pid);
    }
}

/* Update the protocols property of given window. */
static void x_update_window_protocols(XProperties *properties)
{
    xcb_get_property_cookie_t protocols_cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;

    protocols_cookie = xcb_icccm_get_wm_protocols(connection,
            properties->window, ATOM(WM_PROTOCOLS));
    if (!xcb_icccm_get_wm_protocols_reply(connection, protocols_cookie,
                &protocols, NULL)) {
        properties->protocols = xcalloc(1, sizeof(*properties->protocols));
        return;
    }

    properties->protocols = xmalloc(sizeof(*protocols.atoms) *
            (protocols.atoms_len + 1));
    memcpy(properties->protocols, protocols.atoms, sizeof(*protocols.atoms) *
            protocols.atoms_len);
    properties->protocols[protocols.atoms_len] = XCB_NONE;

    xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
}

/* Update the fullscreen monitors property of given window. */
static void x_update_window_fullscreen_monitors(XProperties *properties)
{
    xcb_get_property_reply_t *monitors;

    monitors = x_get_property(properties->window,
            ATOM(_NET_WM_FULLSCREEN_MONITORS), XCB_ATOM_CARDINAL, 32,
            sizeof(properties->fullscreen_monitor) / sizeof(uint32_t), NULL);
    if (monitors == NULL) {
        memset(&properties->fullscreen_monitor, 0,
                sizeof(properties->fullscreen_monitor));
    } else {
        properties->fullscreen_monitor =
            *(Extents*) xcb_get_property_value(monitors);
        free(monitors);
    }
}

/* Update the property with @properties corresponding to given atom. */
bool x_cache_window_property(XProperties *properties, xcb_atom_t atom)
{
    /* this is spaced out because it was very difficult to read with the eyes */
    if (atom == XCB_ATOM_WM_NAME || atom == ATOM(_NET_WM_NAME)) {

        x_update_window_name(properties);

    } else if (atom == XCB_ATOM_WM_NORMAL_HINTS ||
            atom == XCB_ATOM_WM_SIZE_HINTS) {

        x_update_window_size_hints(properties);

    } else if (atom == XCB_ATOM_WM_HINTS) {

        x_update_window_hints(properties);

    } else if (atom == ATOM(_NET_WM_STRUT) ||
            atom == ATOM(_NET_WM_STRUT_PARTIAL)) {

        x_update_window_strut(properties);

    } else if (atom == ATOM(_NET_WM_WINDOW_TYPE)) {

        x_update_window_type(properties);

    } else if (atom == ATOM(_NET_WM_STATE)) {

        x_update_window_state(properties);

    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {

        x_update_window_transient_for(properties);

    } else if (atom == ATOM(_NET_WM_PID)) {

        x_update_window_pid(properties);

    } else if (atom == ATOM(WM_PROTOCOLS)) {

        x_update_window_protocols(properties);

    } else if (atom == ATOM(_NET_WM_FULLSCREEN_MONITORS)) {

        x_update_window_fullscreen_monitors(properties);

    } else {
        return false;
    }
    return true;
}

/* Initialize all properties within @properties. */
void x_init_properties(XProperties *properties, xcb_window_t window)
{
    static void (*const updaters[])(XProperties *properties) = {
        x_update_window_name,
        x_update_window_size_hints,
        x_update_window_hints,
        x_update_window_strut,
        x_update_window_type,
        x_update_window_state,
        x_update_window_transient_for,
        x_update_window_pid,
        x_update_window_protocols,
        x_update_window_fullscreen_monitors,
    };

    properties->window = window;
    for (uint32_t i = 0; i < SIZE(updaters); i++) {
        updaters[i](properties);
    }
}
