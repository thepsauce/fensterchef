#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <xcb/randr.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "root_properties.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* This file handles all kinds of X events.
 *
 * Note the difference between REQUESTS and NOTIFICATIONS.
 * REQUEST: What is requested has not happened yet and will not happen
 * until the window manager does something.
 *
 * NOTIFICATIONS: What is notified ALREADY happened, there is nothing
 * to do now but to take note of it.
 */

/* this is the first index of a randr event */
uint8_t randr_event_base;

/* signals whether the alarm signal was received */
volatile sig_atomic_t timer_expired_signal;

/* this is used for moving/resizing a popup window */
static struct {
    /* the window that is being moved */
    Window *window;
    /* how to move or resize the window */
    wm_move_resize_direction_t direction;
    /* initial position and size of the window */
    Rectangle initial_geometry;
    /* the initial position of the mouse */
    Position start;
} move_resize;

/* Handle an incoming alarm. */
static void alarm_handler(int signal)
{
    (void) signal;
    timer_expired_signal = true;
}

/* Create a signal handler for `SIGALRM`. */
int initialize_signal_handlers(void)
{
    struct sigaction action;

    /* install the signal handler for the alarm signal, this is triggered when
     * the alarm created by `alarm()` expires
     */
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        LOG_ERROR(NULL, "could not create alarm handler\n");
        return ERROR;
    }
    return OK;
}

/* Run the next cycle of the event loop. */
int next_cycle(int (*callback)(int cycle_when, xcb_generic_event_t *event))
{
    xcb_generic_event_t *event;
    fd_set set;

    if (!is_fensterchef_running || xcb_connection_has_error(connection) != 0) {
        return ERROR;
    }

    if (callback != NULL && (*callback)(CYCLE_PREPARE, NULL) != OK) {
        return ERROR;
    }

    /* prepare `set` for `select()` */
    FD_ZERO(&set);
    FD_SET(x_file_descriptor, &set);

    /* using select here is key: select will block until data on the file
     * descriptor for the X connection arrives; when a signal is received,
     * `select()` will however also unblock and return -1
     */
    if (select(x_file_descriptor + 1, &set, NULL, NULL, NULL) > 0) {
        /* handle all received events */
        while (event = xcb_poll_for_event(connection), event != NULL) {
            if (callback != NULL && (*callback)(CYCLE_EVENT, event) != OK) {
                return ERROR;
            }

            handle_event(event);

            if (reload_requested) {
                reload_user_configuration();
                reload_requested = false;
            }

            free(event);
        }
    }

    if (timer_expired_signal) {
        LOG("triggered alarm: hiding notification window\n");
        /* hide the notification window */
        xcb_unmap_window(connection, notification_window);
        timer_expired_signal = false;
    }

    /* flush after every event so all changes are reflected */
    xcb_flush(connection);

    return OK;
}

/* Start moving/resizing given popup window. */
void initiate_window_move_resize(Window *window,
        wm_move_resize_direction_t direction,
        int32_t start_x, int32_t start_y)
{
    /* a window is alread being moved/resized */
    if (move_resize.window != NULL) {
        return;
    }

    move_resize.window = window;
    move_resize.direction = direction;
    move_resize.initial_geometry.x = window->position.x;
    move_resize.initial_geometry.y = window->position.y;
    move_resize.initial_geometry.width = window->size.width;
    move_resize.initial_geometry.height = window->size.height;
    move_resize.start.x = start_x;
    move_resize.start.y = start_y;

    /* grab mouse events, we will then receive all mouse events */
    xcb_grab_pointer(connection, false, screen->root,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root,
            XCB_NONE, XCB_CURRENT_TIME);
}

/* Reset the position of the window being moved/resized. */
static void cancel_window_move_resize(void)
{
    Frame *frame;

    if (move_resize.window == NULL) {
        return;
    }

    /* restore the old position and size */
    frame = get_frame_of_window(move_resize.window);
    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT,
                move_resize.window->position.x -
                    move_resize.initial_geometry.x);
        bump_frame_edge(frame, FRAME_EDGE_TOP,
                move_resize.window->position.y -
                    move_resize.initial_geometry.y);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT,
                move_resize.initial_geometry.width -
                    move_resize.window->size.width);
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM,
                move_resize.initial_geometry.height -
                    move_resize.window->size.height);
    } else {
        set_window_size(move_resize.window,
                move_resize.initial_geometry.x,
                move_resize.initial_geometry.y,
                move_resize.initial_geometry.width,
                move_resize.initial_geometry.height);
    }

    /* release mouse events back to the applications */
    xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
    move_resize.window = NULL;
}

/* Create notifications are sent when a client creates a window on our
 * connection.
 */
static void handle_create_notify(xcb_create_notify_event_t *event)
{
    Window *window;

    /* ignore the fensterchef windows */
    if (event->window == check_window ||
            event->window == notification_window ||
            event->window == window_list_window) {
        return;
    }

    window = create_window(event->window);
    window->position.x = event->x;
    window->position.y = event->y;
    window->size.width = event->width;
    window->size.height = event->height;
}

/* Map requests are sent when a new window wants to become on screen, this
 * is also where we register new windows and wrap them into the internal
 * Window struct.
 */
static void handle_map_request(xcb_map_request_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }
    show_window(window);
    set_focus_window(window);
}

/* Button press events are sent when the mouse is pressed together with
 * the special modifier key. This is used to move popup windows.
 */
static void handle_button_press(xcb_button_press_event_t *event)
{
    Window *window;
    struct configuration_button *button;

    if (move_resize.window != NULL) {
        cancel_window_move_resize();
        return;
    }

    if (event->child == 0) {
        window = NULL;
    } else {
        window = get_window_of_xcb_window(event->child);
        if (window == NULL) {
            return;
        }
    }

    button = find_configured_button(&configuration, event->state,
            event->detail, 0);
    if (button != NULL) {
        LOG("performing %" PRIu32 " action(s)\n", button->number_of_actions);
        for (uint32_t i = 0; i < button->number_of_actions; i++) {
            LOG("performing action %s\n",
                    convert_action_to_string(button->actions[i].code));
            do_action(&button->actions[i], window);
        }

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, event->time);
        }
    }
}

/* Button releases are only sent when we grabbed them. This only happens
 * when a popup window is being moved/resized.
 */
static void handle_button_release(xcb_button_release_event_t *event)
{
    Window *window;
    struct configuration_button *button;

    if (event->child == 0) {
        window = NULL;
    } else {
        window = get_window_of_xcb_window(event->child);
        if (window == NULL) {
            return;
        }
    }

    if (move_resize.window != NULL) {
        /* release mouse events back to the applications */
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        move_resize.window = NULL;
    }

    button = find_configured_button(&configuration, event->state,
            event->detail, BINDING_FLAG_RELEASE);
    if (button != NULL) {
        LOG("performing %" PRIu32 " action(s)\n", button->number_of_actions);
        for (uint32_t i = 0; i < button->number_of_actions; i++) {
            LOG("performing action %s\n",
                    convert_action_to_string(button->actions[i].code));
            do_action(&button->actions[i], window);
        }

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, event->time);
        }
    }
}

/* Motion notifications (mouse move events) are only sent when we grabbed them.
 * This only happens when a popup window is being moved.
 */
static void handle_motion_notify(xcb_motion_notify_event_t *event)
{
    Rectangle new_geometry;
    Frame *frame;

    if (move_resize.window == NULL) {
        return;
    }

    if (move_resize.direction == 0) {
        /* TODO: figure out the best direction */
        move_resize.direction = _NET_WM_MOVERESIZE_MOVE;
        move_resize.start.x = event->root_x;
        move_resize.start.y = event->root_y;
        return;
    }

    new_geometry = move_resize.initial_geometry;
    switch (move_resize.direction) {
    /* the top left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
        new_geometry.x -= move_resize.start.x - event->root_x;
        new_geometry.y -= move_resize.start.y - event->root_y;
        new_geometry.width += move_resize.start.x - event->root_x;
        new_geometry.height += move_resize.start.y - event->root_y;
        break;

    /* the top size of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOP:
        new_geometry.y -= move_resize.start.y - event->root_y;
        new_geometry.height += move_resize.start.y - event->root_y;
        break;

    /* the top right corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
        new_geometry.y -= move_resize.start.y - event->root_y;
        new_geometry.width -= move_resize.start.x - event->root_x;
        new_geometry.height += move_resize.start.y - event->root_y;
        break;

    /* the right side of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_RIGHT:
        new_geometry.width -= move_resize.start.x - event->root_x;
        break;

    /* the bottom right corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
        new_geometry.width -= move_resize.start.x - event->root_x;
        new_geometry.height -= move_resize.start.y - event->root_y;
        break;

    /* the bottom side of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
        new_geometry.height -= move_resize.start.y - event->root_y;
        break;

    /* the bottom left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
        new_geometry.x -= move_resize.start.x - event->root_x;
        new_geometry.width += move_resize.start.x - event->root_x;
        new_geometry.height -= move_resize.start.y - event->root_y;
        break;

    /* the bottom left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_LEFT:
        new_geometry.x -= move_resize.start.x - event->root_x;
        new_geometry.width += move_resize.start.x - event->root_x;
        break;

    /* the entire window is being moved */
    case _NET_WM_MOVERESIZE_MOVE:
        new_geometry.x -= move_resize.start.x - event->root_x;
        new_geometry.y -= move_resize.start.y - event->root_y;
        break;

    /* these are not relevant for mouse moving/resizing */
    case _NET_WM_MOVERESIZE_SIZE_KEYBOARD:
    case _NET_WM_MOVERESIZE_MOVE_KEYBOARD:
    case _NET_WM_MOVERESIZE_CANCEL:
        break;
    }

    frame = get_frame_of_window(move_resize.window);
    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT,
                move_resize.window->position.x - new_geometry.x);
        bump_frame_edge(frame, FRAME_EDGE_TOP,
                move_resize.window->position.y - new_geometry.y);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT,
                new_geometry.width - move_resize.window->size.width);
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM,
                new_geometry.height - move_resize.window->size.height);
    } else {
        set_window_size(move_resize.window,
                new_geometry.x,
                new_geometry.y,
                new_geometry.width,
                new_geometry.height);
    }
}

/* Property notifications are sent when a window property changes. */
static void handle_property_notify(xcb_property_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    if (!cache_window_property(&window->properties, event->atom)) {
        /* nothing we know of changed */
        return;
    }

    set_window_mode(window, predict_window_mode(window), false);

    /* check if the strut has changed */
    if (window->state.is_visible &&
            (event->atom == ATOM(_NET_WM_STRUT_PARTIAL) ||
             event->atom == ATOM(_NET_WM_STRUT))) {
        reconfigure_monitor_frame_sizes();
        synchronize_root_property(ROOT_PROPERTY_WORK_AREA);
    }
}

/* Unmap notifications are sent after a window decided it wanted to not be seen
 * anymore.
 */
void handle_unmap_notify(xcb_unmap_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    /* if the currently moved window is unmapped */
    if (window == move_resize.window) {
        /* release mouse events back to the applications */
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        move_resize.window = NULL;
    }

    hide_window(window);
}

/* Destroy notifications are sent when a window leaves the X server.
 * Good bye to that window!
 */
static void handle_destroy_notify(xcb_destroy_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
        destroy_window(window);
    }
}

/* Key press events are sent when a grabbed key is pressed. */
void handle_key_press(xcb_key_press_event_t *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state,
            get_keysym(event->detail), 0);
    if (key != NULL) {
        LOG("performing %" PRIu32 " action(s)\n", key->number_of_actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
            LOG("performing action %s\n",
                    convert_action_to_string(key->actions[i].code));
            do_action(&key->actions[i], focus_window);
        }

        /* make the event pass through to the focused client */
        if ((key->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_KEYBOARD,
                    event->time);
        }
    }
}

/* Key release events are sent when a grabbed key is released. */
void handle_key_release(xcb_key_release_event_t *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state,
            get_keysym(event->detail), BINDING_FLAG_RELEASE);
    if (key != NULL) {
        LOG("performing %" PRIu32 " action(s)\n", key->number_of_actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
            LOG("performing action %s\n",
                    convert_action_to_string(key->actions[i].code));
            do_action(&key->actions[i], focus_window);
        }

        /* make the event pass through to the focused client */
        if ((key->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_KEYBOARD,
                    event->time);
        }
    }
}

/* Configure requests are received when a window wants to choose its own
 * position and size, we just (TODO:) blindly follow for now.
 */
void handle_configure_request(xcb_configure_request_event_t *event)
{
    Window *window;
    int value_index;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    value_index = 0;
    if ((event->value_mask & XCB_CONFIG_WINDOW_X)) {
        general_values[value_index++] = event->x;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_Y)) {
        general_values[value_index++] = event->y;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_WIDTH)) {
        general_values[value_index++] = event->width;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)) {
        general_values[value_index++] = event->height;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
        general_values[value_index++] = event->border_width;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_SIBLING)) {
        general_values[value_index++] = event->sibling;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)) {
        general_values[value_index++] = event->stack_mode;
    }

    xcb_configure_window(connection, event->window, event->value_mask, general_values);
}

/* Configure notifications are sent when a window changed its data. */
void handle_configure_notify(xcb_configure_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    window->position.x = event->x;
    window->position.y = event->y;
    window->size.width = event->width;
    window->size.height = event->height;
}

/* Screen change notifications are sent when the screen configurations is
 * changed, this can include position, size etc.
 */
void handle_screen_change(xcb_randr_screen_change_notify_event_t *event)
{
    (void) event;
    merge_monitors(query_monitors());
}

/* Mapping notifications are sent when the modifier keys or keyboard mapping
 * changes.
 */
void handle_mapping_notify(xcb_mapping_notify_event_t *event)
{
    refresh_keymap(event);
}

/* Client messages are sent by a client to our window manager to request certain
 * things.
 */
void handle_client_message(xcb_client_message_event_t *event)
{
    Window *window;
    int32_t x, y;
    uint32_t width, height;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    /* request to close the window */
    if (event->type == ATOM(_NET_CLOSE_WINDOW)) {
        close_window(window);
    /* request to move and resize the window */
    } else if (event->type == ATOM(_NET_MOVERESIZE_WINDOW)) {
        x = event->data.data32[1];
        y = event->data.data32[2];
        width = event->data.data32[3];
        height = event->data.data32[4];
        adjust_for_window_gravity(
                get_monitor_from_rectangle(x, y, width, height),
                &x, &y, width, height, event->data.data32[0]);
        set_window_size(window, x, y, width, height);
    /* request to dynamically move and resize the window */
    } else if (event->type == ATOM(_NET_WM_MOVERESIZE)) {
        if (window->state.mode != WINDOW_MODE_POPUP) {
            return;
        }
        if (event->data.data32[2] == _NET_WM_MOVERESIZE_CANCEL) {
            cancel_window_move_resize();
            return;
        }
        initiate_window_move_resize(window, event->data.data32[2],
                event->data.data32[0], event->data.data32[1]);
    }
    /* TODO: handle more client messages */
}

/* Handle the given xcb event.
 *
 * Descriptions for each event are above each handler.
 */
void handle_event(xcb_generic_event_t *event)
{
    uint8_t type;

    LOG("");
    log_event(event);

    /* remove the most significant bit, this gets the actual event type */
    type = (event->response_type & ~0x80);

    if (type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        handle_screen_change((xcb_randr_screen_change_notify_event_t*) event);
        return;
    }

    switch (type) {
    /* a window was created */
    case XCB_CREATE_NOTIFY:
        handle_create_notify((xcb_create_notify_event_t*) event);
        break;

    /* a window wants to appear on the screen */
    case XCB_MAP_REQUEST:
        handle_map_request((xcb_map_request_event_t*) event);
        break;

    /* a mouse button was pressed */
    case XCB_BUTTON_PRESS:
        handle_button_press((xcb_button_press_event_t*) event);
        /* continue processing pointer events normally */
        xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                ((xcb_button_press_event_t*) event)->time);
        break;

    /* a mouse button was released */
    case XCB_BUTTON_RELEASE:
        handle_button_release((xcb_button_release_event_t*) event);
        /* continue processing pointer events normally */
        xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                ((xcb_button_release_event_t*) event)->time);
        break;

    /* the mouse was moved */
    case XCB_MOTION_NOTIFY:
        handle_motion_notify((xcb_motion_notify_event_t*) event);
        break;

    /* a window changed a property */
    case XCB_PROPERTY_NOTIFY:
        handle_property_notify((xcb_property_notify_event_t*) event);
        break;

    /* a window was removed from the screen */
    case XCB_UNMAP_NOTIFY:
        handle_unmap_notify((xcb_unmap_notify_event_t*) event);
        break;

    /* a window was destroyed */
    case XCB_DESTROY_NOTIFY:
        handle_destroy_notify((xcb_destroy_notify_event_t*) event);
        break;

    /* a window wants to configure itself */
    case XCB_CONFIGURE_REQUEST:
        handle_configure_request((xcb_configure_request_event_t*) event);
        break;

    /* a window was configured */
    case XCB_CONFIGURE_NOTIFY:
        handle_configure_notify((xcb_configure_notify_event_t*) event);
        break;

    /* a key was pressed */
    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        /* continue processing keyboard events normally */
        xcb_allow_events(connection, XCB_ALLOW_ASYNC_KEYBOARD,
                ((xcb_key_press_event_t*) event)->time);
        break;

    /* a key was released */
    case XCB_KEY_RELEASE:
        handle_key_release((xcb_key_release_event_t*) event);
        /* continue processing keyboard events normally */
        xcb_allow_events(connection, XCB_ALLOW_ASYNC_KEYBOARD,
                ((xcb_key_release_event_t*) event)->time);
        break;

    /* keyboard mapping changed */
    case XCB_MAPPING_NOTIFY:
        handle_mapping_notify((xcb_mapping_notify_event_t*) event);
        break;

    /* a client sent us a message */
    case XCB_CLIENT_MESSAGE:
        handle_client_message((xcb_client_message_event_t*) event);
        break;
    }
}
