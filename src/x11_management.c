#include <inttypes.h>
#include <string.h>

#include "log.h"
#include "fensterchef.h"
#include "window.h"
#include "window_list.h"
#include "x11_management.h"

/* event mask for the root window; with this event mask, we get the following
 * events:
 * SubstructureNotifyMask
 *  CirculateNotify
 * 	ConfigureNotify
 * 	CreateNotify
 * 	DestroyNotify
 * 	GravityNotify
 *  MapNotify
 *  ReparentNotify
 *  UnmapNotify
 * SubstructureRedirectMask
 *  CirculateRequest
 *  ConfigureRequest
 *  MapRequest
 */
#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | \
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY)

/* connection to the xcb server */
xcb_connection_t *connection;

/* file descriptor associated to the X connection */
int x_file_descriptor;

/* the selected x screen */
xcb_screen_t *screen;

/* general purpose values for xcb function calls */
uint32_t general_values[7];

/* supporting wm check window */
xcb_window_t wm_check_window;

/* user notification window */
XClient notification;

struct x_atoms x_atoms[] = {
#define X(atom) { #atom, 0 },
    DEFINE_ALL_ATOMS
#undef X
};

/* Initialize the X server connection and the X atoms. */
int initialize_x11(void)
{
    int connection_error;
    int screen_number;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t i;
    xcb_intern_atom_cookie_t atom_cookies[ATOM_MAX];
    xcb_generic_error_t *error;
    xcb_intern_atom_reply_t *atom;

    /* read the DISPLAY environment variable to determine the display to
     * attach to; if the DISPLAY variable is in the form :X.Y then X is the
     * display number and Y the screen number which is stored in `screen_number`
     */
    connection = xcb_connect(NULL, &screen_number);
    /* standard way to check if a connection failed */
    connection_error = xcb_connection_has_error(connection);
    if (connection_error > 0) {
        LOG_ERROR("could not connect to the X server: %X\n", connection_error);
        return ERROR;
    }

    x_file_descriptor = xcb_get_file_descriptor(connection);

    setup = xcb_get_setup(connection);

    /* iterator over all screens to find the one with the screen number */
    for(i = xcb_setup_roots_iterator(setup); i.rem > 0; xcb_screen_next(&i)) {
        if (screen_number == 0) {
            screen = i.data;
            LOG("%C\n", screen);
            break;
        }
        screen_number--;
    }

    if (screen == NULL) {
        /* this should in theory not happen because `xcb_connect()` already
         * checks if the screen exists
         */
        LOG_ERROR("there is no screen no. %d\n", screen_number);
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
            LOG_ERROR("could not intern atom %s: %E", x_atoms[i].name, error);
            free(error);
            return ERROR;
        }
        x_atoms[i].atom = atom->atom;
        free(atom);
    }
    return OK;
}

/* Create the check, notification and window list windows. */
static int create_utility_windows(void)
{
    const char *notification_name = "[fensterchef] notification";
    xcb_generic_error_t *error;

    /* create the wm check window, this can be used by other applications to
     * identify our window manager, we also use it as fallback focus
     */
    wm_check_window = xcb_generate_id(connection);
    error = xcb_request_check(connection, xcb_create_window_checked(connection,
                XCB_COPY_FROM_PARENT, wm_check_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
                0, NULL));
    if (error != NULL) {
        LOG_ERROR("could not create check window: %E\n", error);
        free(error);
        return ERROR;
    }
    /* set the check window name to the name of fensterchef */
    xcb_icccm_set_wm_name(connection, wm_check_window,
            ATOM(UTF8_STRING), 8, strlen(FENSTERCHEF_NAME), FENSTERCHEF_NAME);
    /* the check window has itself as supporting wm check window */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            wm_check_window, ATOM(_NET_SUPPORTING_WM_CHECK),
            XCB_ATOM_WINDOW, 32, 1, &wm_check_window);

    /* map the window so it can receive focus */
    xcb_map_window(connection, wm_check_window);

    /* create a notification window for showing the user messages */
    notification.id = xcb_generate_id(connection);
    /* indicate to not manage the window */
    general_values[0] = true;
    error = xcb_request_check(connection, xcb_create_window_checked(connection,
                XCB_COPY_FROM_PARENT, notification.id,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                XCB_CW_OVERRIDE_REDIRECT, general_values));
    if (error != NULL) {
        LOG_ERROR("could not create notification window: %E\n", error);
        free(error);
        return ERROR;
    }
    notification.x = -1;
    notification.y = -1;
    notification.width = 1;
    notification.height = 1;
    xcb_icccm_set_wm_name(connection, notification.id,
            ATOM(UTF8_STRING), 8, strlen(notification_name), notification_name);

    /* create the window list */
    if (initialize_window_list() != OK) {
        return ERROR;
    }
    return OK;
}

/* Try to take control of the window manager role. */
int take_control(void)
{
    xcb_generic_error_t *error;

    /* set the mask of the root window so we receive important events like
     * map requests
     */
    general_values[0] = ROOT_EVENT_MASK;
    error = xcb_request_check(connection,
            xcb_change_window_attributes_checked(connection, screen->root,
                XCB_CW_EVENT_MASK, general_values));
    if (error != NULL) {
        LOG_ERROR("could not change root window mask: %E\n", error);
        free(error);
        return ERROR;
    }

    /* create the necessary utility windows */
    if (create_utility_windows() != OK) {
        return ERROR;
    }

    /* intialize the focus */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
            wm_check_window, XCB_CURRENT_TIME);
    return OK;
}

/* Go through all existing windows and manage them. */
void query_existing_windows(void)
{
    xcb_query_tree_cookie_t tree_cookie;
    xcb_query_tree_reply_t *tree;
    xcb_window_t *windows;
    int length;
    Window *window;

    /* get a list of child windows of the root in bottom-to-top stacking order
     */
    tree_cookie = xcb_query_tree(connection, screen->root);
    tree = xcb_query_tree_reply(connection, tree_cookie, NULL);
    /* not sure what this implies, maybe the connection is broken */
    if (tree == NULL) {
        return;
    }

    windows = xcb_query_tree_children(tree);
    length = xcb_query_tree_children_length(tree);
    for (int i = 0; i < length; i++) {
        window = create_window(windows[i]);
        if (window != NULL && window->client.is_mapped) {
            show_window(window);
        }
    }

    free(tree);
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

        ATOM(_NET_WORKAREA),

        ATOM(_NET_SUPPORTING_WM_CHECK),

        ATOM(_NET_CLOSE_WINDOW),

        ATOM(_NET_MOVERESIZE_WINDOW),

        ATOM(_NET_WM_MOVERESIZE),

        ATOM(_NET_RESTACK_WINDOW),

        ATOM(_NET_REQUEST_FRAME_EXTENTS),

        ATOM(_NET_WM_NAME),

        ATOM(_NET_WM_DESKTOP),

        ATOM(_NET_WM_WINDOW_TYPE),
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

        ATOM(_NET_WM_STRUT),
        ATOM(_NET_WM_STRUT_PARTIAL),

        ATOM(_NET_FRAME_EXTENTS),

        ATOM(_NET_WM_FULLSCREEN_MONITORS),
    };
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_SUPPORTED), XCB_ATOM_ATOM, 32,
            SIZE(supported_atoms), supported_atoms);

    /* the wm check window */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            screen->root, ATOM(_NET_SUPPORTING_WM_CHECK), XCB_ATOM_WINDOW,
            32, 1, &wm_check_window);


    /* set the active window */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_ACTIVE_WINDOW), XCB_ATOM_WINDOW, 32, 1, &screen->root);

    /* set the work area */
    const Rectangle workarea = {
        0, 0,
        screen->width_in_pixels, screen->height_in_pixels
    };
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_WORKAREA), XCB_ATOM_CARDINAL, 32, 4, &workarea);
}

/* Show the client on the X server. */
void map_client(XClient *client)
{
    if (client->is_mapped) {
        return;
    }

    LOG("showing client %w\n", client->id);

    client->is_mapped = true;

    xcb_map_window(connection, client->id);
}

/* Hide the client on the X server. */
void unmap_client(XClient *client)
{
    if (!client->is_mapped) {
        return;
    }

    LOG("hiding client %w\n", client->id);

    client->is_mapped = false;

    xcb_unmap_window(connection, client->id);
}

/* Set the size of a window associated to the X server. */
void configure_client(XClient *client, int32_t x, int32_t y, uint32_t width,
        uint32_t height, uint32_t border_width)
{
    if (client->x == x && client->y == y && client->width == width &&
            client->height == height && client->border_width == border_width) {
        return;
    }

    LOG("configuring client %w to %R %" PRIu32 "\n", client->id,
            x, y, width, height, border_width);

    client->x = x;
    client->y = y;
    client->width = width;
    client->height = height;
    client->border_width = border_width;

    general_values[0] = x;
    general_values[1] = y;
    general_values[2] = width;
    general_values[3] = height;
    general_values[4] = border_width;
    xcb_configure_window(connection, client->id,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
}

/* Set the client border color. */
void change_client_attributes(XClient *client, uint32_t border_color)
{
    if (client->border_color == border_color) {
        return;
    }
    client->border_color = border_color;

    LOG("changing attributes of client %w to %#x\n", client->id,
            (unsigned) border_color);

    general_values[0] = border_color;
    xcb_change_window_attributes(connection, client->id,
            XCB_CW_BORDER_PIXEL, general_values);
}

/* Wrapper around getting a cookie and reply for a GetProperty request. */
static inline xcb_get_property_reply_t *get_property(xcb_window_t window,
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
        LOG("window %w has misformatted property %a\n", window, property);
        free(reply);
        return NULL;
    }
    return reply;
}

/* Update the name within @properties. */
static void update_window_name(Window *window)
{
    xcb_get_property_reply_t *name;

    free(window->name);

    name = get_property(window->client.id, ATOM(_NET_WM_NAME),
            XCB_GET_PROPERTY_TYPE_ANY, 8, UINT32_MAX, NULL);
    if (name == NULL) {
        /* fall back to `WM_NAME` */
        name = get_property(window->client.id, XCB_ATOM_WM_NAME,
                XCB_GET_PROPERTY_TYPE_ANY, 8, UINT32_MAX, NULL);
        if (name == NULL) {
            window->name = NULL;
            return;
        }
    }

    window->name = (utf8_t*) xstrndup(
            xcb_get_property_value(name),
            xcb_get_property_value_length(name));

    free(name);
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
    wm_strut_partial_t new_strut;
    xcb_get_property_reply_t *strut;

    memset(&new_strut, 0, sizeof(new_strut));

    strut = get_property(window->client.id,
            ATOM(_NET_WM_STRUT_PARTIAL), XCB_ATOM_CARDINAL, 32,
            sizeof(wm_strut_partial_t) / sizeof(uint32_t), NULL);
    if (strut == NULL) {
        /* `_NET_WM_STRUT` is older than `_NET_WM_STRUT_PARTIAL`, fall back to
         * it when there is no strut partial
         */
        strut = get_property(window->client.id,
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

    window->strut = new_strut;
}

/* Get a window property as list of atoms. */
static inline xcb_atom_t *get_atom_list(xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_atom_t *atoms;

    cookie = xcb_get_property(connection, 0, window, atom, XCB_ATOM_ATOM,
            0, UINT32_MAX);
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
            sizeof(window->fullscreen_monitors) / sizeof(uint32_t), NULL);
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
            sizeof(window->motif_wm_hints) / sizeof(uint32_t), NULL);
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
        if (atoms[i] == ATOM(_NET_WM_STATE)) {
            states = get_atom_list(window->client.id, ATOM(_NET_WM_STATE));
        } else if (atoms[i] == ATOM(_NET_WM_WINDOW_TYPE)) {
            types = get_atom_list(window->client.id, ATOM(_NET_WM_WINDOW_TYPE));
        } else {
            cache_window_property(window, atoms[i]);
        }
    }

    /* these are two direct checks */
    if (is_atom_included(states, ATOM(_NET_WM_STATE_FULLSCREEN))) {
        predicted_mode = WINDOW_MODE_FULLSCREEN;
    } else if (is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_DOCK))) {
        predicted_mode = WINDOW_MODE_DOCK;
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
