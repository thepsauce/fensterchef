#include <string.h>

#include "configuration.h"
#include "log.h"
#include "utility.h"
#include "window.h"
#include "window_properties.h"

struct x_atoms x_atoms[] = {
#define X(atom) { #atom, 0 },
    DEFINE_ALL_ATOMS
#undef X
};

/* Intern all X atoms we require. */
int initialize_atoms(void)
{
    xcb_intern_atom_cookie_t atom_cookies[ATOM_MAX];
    xcb_generic_error_t *error;
    xcb_intern_atom_reply_t *atom;
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
            LOG_ERROR("could not intern atom %s: %E", x_atoms[i].name, error);
            free(error);
            return ERROR;
        }
        x_atoms[i].atom = atom->atom;
        free(atom);
    }
    return OK;
}

/* Set the initial root window properties. */
void initialize_root_properties(void)
{
    /* set the supported ewmh atoms of our window manager, ewmh support was easy
     * to add with the help of:
     * https://specifications.freedesktop.org/wm-spec/latest/index.html#id-1.2
     */
    const xcb_atom_t supported_atoms[] = {
        ATOM(_NET_SUPPORTED),

        ATOM(_NET_CLIENT_LIST),
        ATOM(_NET_CLIENT_LIST_STACKING),

        ATOM(_NET_ACTIVE_WINDOW),

        ATOM(_NET_SUPPORTING_WM_CHECK),

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
        ATOM(_NET_WM_STATE_MAXIMIZED_VERT),
        ATOM(_NET_WM_STATE_MAXIMIZED_HORZ),
        ATOM(_NET_WM_STATE_FULLSCREEN),
        ATOM(_NET_WM_STATE_HIDDEN),
        ATOM(_NET_WM_STATE_FOCUSED),

        ATOM(_NET_WM_STRUT),
        ATOM(_NET_WM_STRUT_PARTIAL),

        ATOM(_NET_FRAME_EXTENTS),

        ATOM(_NET_WM_FULLSCREEN_MONITORS),
    };
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_SUPPORTED), XCB_ATOM_ATOM, 32,
            SIZE(supported_atoms), supported_atoms);

    /* the wm check window */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_SUPPORTING_WM_CHECK), XCB_ATOM_WINDOW,
            32, 1, &wm_check_window);


    /* set the active window */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_ACTIVE_WINDOW), XCB_ATOM_WINDOW, 32, 1, &screen->root);
}


/* Wrapper around getting a cookie and reply for a GetProperty request. */
static inline xcb_get_property_reply_t *get_property(
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint32_t format, uint32_t length)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    cookie = xcb_get_property(connection, false, window, property, type, 0,
            length);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    /* check if the property is in the needed format and if it is long enough */
    if (reply != NULL && (reply->format != format || (uint32_t)
            xcb_get_property_value_length(reply) < length * format / 8)) {
        LOG("window %w has misformatted property %a\n", window, property);
        free(reply);
        reply = NULL;
    }
    return reply;
}

/* Wrapper around getting a cookie and reply for a GetProperty request. */
static inline xcb_get_property_reply_t *get_text_property(xcb_window_t window,
        xcb_atom_t property)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    cookie = xcb_get_property(connection, false, window, property,
            XCB_GET_PROPERTY_TYPE_ANY, 0, 2048);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    /* check if the property is in the needed format */
    if (reply != NULL && reply->format != 8) {
        LOG("window %w has misformatted property %a\n", window, property);
        free(reply);
        reply = NULL;
    }
    return reply;
}

/* Gets the `FENSTERCHEF_COMMAND` property from @window. */
char *get_fensterchef_command_property(xcb_window_t window)
{
    xcb_get_property_reply_t *command_property;
    char *command = NULL;

    command_property = get_text_property(window, ATOM(FENSTERCHEF_COMMAND));
    if (command_property != NULL) {
        command = xstrndup(
                xcb_get_property_value(command_property),
                xcb_get_property_value_length(command_property));
        free(command_property);
    }

    return command;
}

/* Update the name within @properties. */
static void update_window_name(Window *window)
{
    xcb_get_property_reply_t *name;

    free(window->name);
    window->name = NULL;

    name = get_text_property(window->client.id, ATOM(_NET_WM_NAME));
    /* try to fall back to `WM_NAME` */
    if (name == NULL) {
        name = get_text_property(window->client.id, XCB_ATOM_WM_NAME);
    }

    if (name != NULL) {
        window->name = (utf8_t*) xstrndup(
                xcb_get_property_value(name),
                xcb_get_property_value_length(name));

        free(name);
    }
}

/* Update the size_hints within @properties. */
static void update_window_size_hints(Window *window)
{
    xcb_get_property_cookie_t size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(connection,
            window->client.id, XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(connection, size_hints_cookie,
                &window->size_hints, NULL)) {
        window->size_hints.flags = 0;
    }
}

/* Update the hints within @properties. */
static void update_window_hints(Window *window)
{
    xcb_get_property_cookie_t hints_cookie;

    hints_cookie = xcb_icccm_get_wm_hints(connection, window->client.id);
    if (!xcb_icccm_get_wm_hints_reply(connection, hints_cookie,
                &window->hints, NULL)) {
        window->hints.flags = 0;
    }
}

/* Update the strut partial property within @properties. */
static void update_window_strut(Window *window)
{
    xcb_get_property_reply_t *strut;

    memset(&window->strut, 0, sizeof(window->strut));

    strut = get_property(window->client.id,
            ATOM(_NET_WM_STRUT_PARTIAL), XCB_ATOM_CARDINAL, 32,
            sizeof(wm_strut_partial_t) / sizeof(uint32_t));
    if (strut == NULL) {
        /* `_NET_WM_STRUT` is older than `_NET_WM_STRUT_PARTIAL`, fall back to
         * it when there is no strut partial
         */
        strut = get_property(window->client.id,
                ATOM(_NET_WM_STRUT), XCB_ATOM_CARDINAL, 32,
                sizeof(Extents) / sizeof(uint32_t));
        if (strut != NULL) {
            window->strut.reserved = *(Extents*) xcb_get_property_value(strut);
        }
    } else {
        window->strut = *(wm_strut_partial_t*) xcb_get_property_value(strut);
    }

    free(strut);
}

/* Get a window property as list of atoms. */
static inline xcb_atom_t *get_atom_list(xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_atom_t *atoms;

    /* get up to 32 atoms from this property */
    cookie = xcb_get_property(connection, 0, window, atom, XCB_ATOM_ATOM,
            0, sizeof(atom) * 32);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    if (reply == NULL) {
        return NULL;
    }
    atoms = xmalloc(xcb_get_property_value_length(reply) + sizeof(*atoms));
    memcpy(atoms, xcb_get_property_value(reply),
            xcb_get_property_value_length(reply));
    atoms[xcb_get_property_value_length(reply) / sizeof(xcb_atom_t)] = XCB_NONE;
    free(reply);
    return atoms;
}

/* Update the `transient_for` property within @properties. */
static void update_window_transient_for(Window *window)
{
    xcb_get_property_cookie_t transient_for_cookie;

    transient_for_cookie = xcb_icccm_get_wm_transient_for(connection,
            window->client.id);
    if (!xcb_icccm_get_wm_transient_for_reply(connection, transient_for_cookie,
                &window->transient_for, NULL)) {
        window->transient_for = XCB_NONE;
    }
}

/* Update the `protocols` property within @properties. */
static void update_window_protocols(Window *window)
{
    free(window->protocols);
    window->protocols = get_atom_list(window->client.id,
            ATOM(WM_PROTOCOLS));
}

/* Update the `fullscreen_monitors` property within @properties. */
static void update_window_fullscreen_monitors(Window *window)
{
    xcb_get_property_reply_t *monitors;

    monitors = get_property(window->client.id,
            ATOM(_NET_WM_FULLSCREEN_MONITORS), XCB_ATOM_CARDINAL, 32,
            sizeof(window->fullscreen_monitors) / sizeof(uint32_t));
    if (monitors == NULL) {
        memset(&window->fullscreen_monitors, 0,
                sizeof(window->fullscreen_monitors));
    } else {
        window->fullscreen_monitors =
            *(Extents*) xcb_get_property_value(monitors);
        free(monitors);
    }
}

/* Update the `motif_wm_hints` within @properties. */
static void update_motif_wm_hints(Window *window)
{
    xcb_get_property_reply_t *motif_wm_hints;

    motif_wm_hints = get_property(window->client.id,
            ATOM(_MOTIF_WM_HINTS), ATOM(_MOTIF_WM_HINTS), 32,
            sizeof(window->motif_wm_hints) / sizeof(uint32_t));
    if (motif_wm_hints == NULL) {
        window->motif_wm_hints.flags = 0;
    } else {
        window->motif_wm_hints =
            *(motif_wm_hints_t*) xcb_get_property_value(motif_wm_hints);
        free(motif_wm_hints);
    }
}

/* Update the property within @window corresponding to given atom. */
bool cache_window_property(Window *window, xcb_atom_t atom)
{
    /* this is spaced out because it was very difficult to read with the eyes */
    if (atom == XCB_ATOM_WM_NAME || atom == ATOM(_NET_WM_NAME)) {

        update_window_name(window);

    } else if (atom == XCB_ATOM_WM_NORMAL_HINTS ||
            atom == XCB_ATOM_WM_SIZE_HINTS) {

        update_window_size_hints(window);

    } else if (atom == XCB_ATOM_WM_HINTS) {

        update_window_hints(window);

    } else if (atom == ATOM(_NET_WM_STRUT) ||
            atom == ATOM(_NET_WM_STRUT_PARTIAL)) {

        update_window_strut(window);

    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {

        update_window_transient_for(window);

    } else if (atom == ATOM(WM_PROTOCOLS)) {

        update_window_protocols(window);

    } else if (atom == ATOM(_NET_WM_FULLSCREEN_MONITORS)) {

        update_window_fullscreen_monitors(window);

    } else if (atom == ATOM(_MOTIF_WM_HINTS)) {

        update_motif_wm_hints(window);

    } else {
        return false;
    }
    return true;
}

/* Check if an atom is within the given list of atoms. */
static bool is_atom_included(const xcb_atom_t *atoms, xcb_atom_t atom)
{
    if (atoms == NULL) {
        return false;
    }
    for (; atoms[0] != XCB_NONE; atoms++) {
        if (atoms[0] == atom) {
            return true;
        }
    }
    return false;
}

/* Initialize all properties within @properties. */
window_mode_t initialize_window_properties(Window *window)
{
    xcb_list_properties_cookie_t list_properties_cookie;
    xcb_list_properties_reply_t *list_properties;
    xcb_get_property_reply_t *wm_class;
    const char *wm_class_value;
    int wm_class_length;
    int instance_length;
    utf8_t *instance_name = NULL;
    utf8_t *class_name = NULL;
    xcb_atom_t *atoms;
    int atom_count;
    xcb_atom_t *states = NULL;
    xcb_atom_t *types = NULL;
    window_mode_t predicted_mode = WINDOW_MODE_TILING;

    /* get a list of properties currently set on the window */
    list_properties_cookie = xcb_list_properties(connection, window->client.id);
    list_properties = xcb_list_properties_reply(connection,
            list_properties_cookie, NULL);
    if (list_properties == NULL) {
        return predicted_mode;
    }

    /* cache all properties */
    atoms = xcb_list_properties_atoms(list_properties);
    atom_count = xcb_list_properties_atoms_length(list_properties);
    for (int i = 0; i < atom_count; i++) {
        if (atoms[i] == XCB_ATOM_WM_CLASS) {
            wm_class = get_text_property(window->client.id, XCB_ATOM_WM_CLASS);
            wm_class_value = xcb_get_property_value(wm_class);
            wm_class_length = xcb_get_property_value_length(wm_class);

            instance_length = strnlen((char*) wm_class_value, wm_class_length);
            instance_name = xmalloc(instance_length + 1);
            memcpy(instance_name, wm_class_value, instance_length);
            instance_name[instance_length] = '\0';

            class_name = (utf8_t*) xstrndup(
                    &wm_class_value[instance_length + 1],
                    wm_class_length - instance_length);

            free(wm_class);
        } else if (atoms[i] == ATOM(_NET_WM_STATE)) {
            states = get_atom_list(window->client.id, ATOM(_NET_WM_STATE));
        } else if (atoms[i] == ATOM(_NET_WM_WINDOW_TYPE)) {
            types = get_atom_list(window->client.id, ATOM(_NET_WM_WINDOW_TYPE));
        } else {
            cache_window_property(window, atoms[i]);
        }
    }

    /* these are three direct checks */
    if (is_atom_included(states, ATOM(_NET_WM_STATE_FULLSCREEN))) {
        predicted_mode = WINDOW_MODE_FULLSCREEN;
    } else if (is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_DOCK))) {
        predicted_mode = WINDOW_MODE_DOCK;
    } else if (is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_DESKTOP))) {
        predicted_mode = WINDOW_MODE_DESKTOP;
    /* if this window has strut, it must be a dock window */
    } else if (!is_strut_empty(&window->strut)) {
        predicted_mode = WINDOW_MODE_DOCK;
    /* transient windows are floating windows */
    } else if (window->transient_for != 0) {
        predicted_mode = WINDOW_MODE_FLOATING;
    /* floating windows have an equal minimum and maximum size */
    } else if ((window->size_hints.flags &
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
                 XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) ==
                (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
                 XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) &&
            (window->size_hints.min_width ==
                window->size_hints.max_width ||
            window->size_hints.min_height ==
                window->size_hints.max_height)) {
        predicted_mode = WINDOW_MODE_FLOATING;
    /* floating windows have a window type that is not the normal window type */
    } else if (types != NULL &&
            !is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_NORMAL))) {
        predicted_mode = WINDOW_MODE_FLOATING;
    }

    window->states = states;

    /* if the class property is set, try to find an association */
    if (instance_name != NULL && class_name != NULL) {
        for (uint32_t i = 0;
                i < configuration.assignment.number_of_associations;
                i++) {
            struct configuration_association *association =
                &configuration.assignment.associations[i];
            if (matches_pattern((char*) class_name,
                        (char*) association->class_pattern) &&
                    matches_pattern((char*) instance_name,
                        (char*) association->instance_pattern)) {
                window->number = association->number;
                break;
            }
        }
    }

    free(instance_name);
    free(class_name);
    free(types);
    free(list_properties);

    return predicted_mode;
}

/* Check if @properties includes @protocol. */
bool supports_protocol(Window *window, xcb_atom_t protocol)
{
    return is_atom_included(window->protocols, protocol);
}

/* Check if @properties includes @state. */
bool has_state(Window *window, xcb_atom_t state)
{
    return is_atom_included(window->states, state);
}
