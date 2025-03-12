#include <inttypes.h>
#include <string.h>

#include "log.h"
#include "fensterchef.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"
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
 *
 * And button press/release events.
 */
#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | \
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | \
                         XCB_EVENT_MASK_BUTTON_PRESS | \
                         XCB_EVENT_MASK_BUTTON_RELEASE)

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

/* Initialize the X server connection. */
int initialize_x11(void)
{
    int connection_error;
    int screen_number;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t i;

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
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wm_check_window,
            ATOM(_NET_SUPPORTING_WM_CHECK), XCB_ATOM_WINDOW,
            32, 1, &wm_check_window);

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
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
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

/* Set the input focus to @window. This window may be `NULL`. */
void set_input_focus(Window *window)
{
    xcb_window_t focus_id = XCB_NONE;
    xcb_window_t active_id;
    xcb_atom_t state_atom;

    if (window == NULL) {
        LOG("removed focus from all windows\n");
        active_id = screen->root;

        focus_id = wm_check_window;
    } else {
        active_id = window->client.id;

        state_atom = ATOM(_NET_WM_STATE_FOCUSED);
        add_window_states(window, &state_atom, 1);

        /* if the window wants no focus itself */
        if ((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT) &&
                window->hints.input == 0) {
            char event_data[32];
            xcb_client_message_event_t *event;

            /* bake an event for running a protocol on the window */
            event = (xcb_client_message_event_t*) event_data;
            event->response_type = XCB_CLIENT_MESSAGE;
            event->window = window->client.id;
            event->type = ATOM(WM_PROTOCOLS);
            event->format = 32;
            memset(&event->data, 0, sizeof(event->data));
            event->data.data32[0] = ATOM(WM_TAKE_FOCUS);
            event->data.data32[1] = XCB_CURRENT_TIME;
            xcb_send_event(connection, false, window->client.id,
                    XCB_EVENT_MASK_NO_EVENT, event_data);

            LOG("focusing client: %w through sending WM_TAKE_FOCUS\n",
                    window->client.id);
        } else {
            focus_id = window->client.id;
        }
    }

    if (focus_id != XCB_NONE) {
        LOG("focusing client: %w\n", focus_id);
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
                focus_id, XCB_CURRENT_TIME);
    }

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_ACTIVE_WINDOW), XCB_ATOM_WINDOW, 32, 1, &active_id);
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
