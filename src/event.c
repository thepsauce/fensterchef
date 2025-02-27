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

/* this is used for moving/resizing a floating window */
static struct {
    /* the window that is being moved */
    Window *window;
    /* how to move or resize the window */
    wm_move_resize_direction_t direction;
    /* initial position and size of the window */
    Rectangle initial_geometry;
    /* the initial position of the mouse */
    Point start;
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
        LOG_ERROR("could not create alarm handler\n");
        return ERROR;
    }
    /* I also thought of adding a signal handler for SIGCHLD which gets
     * triggered when a child process finishes but my assumption is that the
     * children get cleaned up automatically when not specifying any handler
     */
    return OK;
}

/* Run the next cycle of the event loop. */
int next_cycle(int (*callback)(int cycle_when, xcb_generic_event_t *event))
{
    int connection_error;
    Window *old_focus_window;
    xcb_generic_event_t *event;
    fd_set set;

    connection_error = xcb_connection_has_error(connection);
    if (!is_fensterchef_running || connection_error > 0) {
        if (connection_error > 0) {
            LOG("connection error: %X\n", connection_error);
        }
        return ERROR;
    }

    if (callback != NULL && (*callback)(CYCLE_PREPARE, NULL) != OK) {
        return ERROR;
    }

    old_focus_window = focus_window;

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

        /* give the new window focus */
        if (old_focus_window != focus_window) {
            if (focus_window == NULL) {
                LOG("removed focus from all windows\n");
                xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                        check_window, XCB_CURRENT_TIME);
            } else {
                general_values[0] = configuration.border.focus_color;
                xcb_change_window_attributes(connection,
                        focus_window->properties.window, XCB_CW_BORDER_PIXEL,
                        general_values);

                xcb_set_input_focus_checked(connection, XCB_INPUT_FOCUS_NONE,
                        focus_window->properties.window, XCB_CURRENT_TIME);
            }
            synchronize_root_property(ROOT_PROPERTY_ACTIVE_WINDOW);
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

/* Start moving/resizing given window. */
void initiate_window_move_resize(Window *window,
        wm_move_resize_direction_t direction,
        int32_t start_x, int32_t start_y)
{
    xcb_query_pointer_cookie_t query_cookie;
    xcb_query_pointer_reply_t *query;
    xcb_grab_pointer_cookie_t grab_cookie;
    xcb_grab_pointer_reply_t *grab;
    xcb_generic_error_t *error;

    /* check if no window is already being moved/resized */
    if (move_resize.window != NULL) {
        return;
    }

    LOG("starting to move/resize %W\n", window);

    /* get the mouse position if the caller does not supply it */
    if (start_x < 0) {
        query_cookie = xcb_query_pointer(connection, screen->root);
        query = xcb_query_pointer_reply(connection, query_cookie, &error);
        if (query == NULL) {
            LOG_ERROR("could not query pointer: %E\n", error);
            free(error);
            return;
        }
        start_x = query->root_x;
        start_y = query->root_y;
        free(query);
    }

    /* figure out a good direction if the caller does not provide one */
    if (direction == _NET_WM_MOVERESIZE_AUTO) {
        const uint32_t border = 2 * configuration.border.size;
        /* check if the mouse is at the top */
        if (start_y < window->y + configuration.mouse.resize_tolerance) {
            if (start_x < window->x + configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
            } else if (start_x - (int32_t) (border + window->width) >=
                    window->x - configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
            } else {
                direction = _NET_WM_MOVERESIZE_SIZE_TOP;
            }
        /* check if the mouse is at the bottom */
        } else if (start_y - (int32_t) (border + window->height) >=
                window->y - configuration.mouse.resize_tolerance) {
            if (start_x < window->x + configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
            } else if (start_x - (int32_t) (border + window->width) >=
                    window->x - configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
            } else {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
            }
        /* check if the mouse is left */
        } else if (start_x < window->x + configuration.mouse.resize_tolerance) {
            direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
        /* check if the mouse is right */
        } else if (start_x - (int32_t) (border + window->width) >=
                window->x - configuration.mouse.resize_tolerance) {
            direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
        /* fall back to simply moving (the mouse is within the window) */
        } else {
            direction = _NET_WM_MOVERESIZE_MOVE;
        }
    }

    move_resize.window = window;
    move_resize.direction = direction;
    move_resize.initial_geometry.x = window->x;
    move_resize.initial_geometry.y = window->y;
    move_resize.initial_geometry.width = window->width;
    move_resize.initial_geometry.height = window->height;
    move_resize.start.x = start_x;
    move_resize.start.y = start_y;

    /* grab mouse events, we will then receive all mouse events */
    grab_cookie = xcb_grab_pointer(connection, false, screen->root,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root,
            XCB_NONE, XCB_CURRENT_TIME);
    grab = xcb_grab_pointer_reply(connection, grab_cookie, &error);
    if (grab == NULL) {
        LOG_ERROR("could not grab pointer: %E\n", error);
        free(error);
        move_resize.window = NULL;
        return;
    }
    if (grab->status != XCB_GRAB_STATUS_SUCCESS) {
        LOG_ERROR("could not grab pointer\n");
        free(grab);
        move_resize.window = NULL;
        return;
    }
    free(grab);
}

/* Reset the position of the window being moved/resized. */
static void cancel_window_move_resize(void)
{
    Frame *frame;

    /* make sure a window is currently being moved/resized */
    if (move_resize.window == NULL) {
        return;
    }

    LOG("cancelling move/resize for %W\n", move_resize.window);

    /* restore the old position and size */
    frame = get_frame_of_window(move_resize.window);
    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT,
                move_resize.window->x -
                    move_resize.initial_geometry.x);
        bump_frame_edge(frame, FRAME_EDGE_TOP,
                move_resize.window->y -
                    move_resize.initial_geometry.y);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT,
                move_resize.initial_geometry.width -
                    move_resize.window->width);
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM,
                move_resize.initial_geometry.height -
                    move_resize.window->height);
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

/* Key press events are sent when a grabbed key is pressed. */
static void handle_key_press(xcb_key_press_event_t *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state,
            get_keysym(event->detail), 0);
    if (key != NULL) {
        LOG("performing action(s): %A\n", key->number_of_actions,
                key->actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
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
static void handle_key_release(xcb_key_release_event_t *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state,
            get_keysym(event->detail), BINDING_FLAG_RELEASE);
    if (key != NULL) {
        LOG("performing action(s): %A\n", key->number_of_actions,
                key->actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
            do_action(&key->actions[i], focus_window);
        }

        /* make the event pass through to the focused client */
        if ((key->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_KEYBOARD,
                    event->time);
        }
    }
}

/* Button press events are sent when a grabbed button is pressed. */
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
        LOG("performing action(s): %A\n", button->number_of_actions,
                button->actions);
        for (uint32_t i = 0; i < button->number_of_actions; i++) {
            do_action(&button->actions[i], window);
        }

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, event->time);
        }
    }
}

/* Button releases are sent when a grabbed button is released. */
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
        LOG("performing action(s): %A\n", button->number_of_actions,
                button->actions);
        for (uint32_t i = 0; i < button->number_of_actions; i++) {
            do_action(&button->actions[i], window);
        }

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, event->time);
        }
    }
}

/* Motion notifications (mouse move events) are only sent when we grabbed them.
 * This only happens when a floating window is being moved.
 */
static void handle_motion_notify(xcb_motion_notify_event_t *event)
{
    Rectangle new_geometry;
    Frame *frame;

    if (move_resize.window == NULL) {
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
    case _NET_WM_MOVERESIZE_AUTO:
        break;
    }

    frame = get_frame_of_window(move_resize.window);
    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT,
                move_resize.window->x - new_geometry.x);
        bump_frame_edge(frame, FRAME_EDGE_TOP,
                move_resize.window->y - new_geometry.y);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT,
                (new_geometry.x + new_geometry.width) -
                (move_resize.window->x + move_resize.window->width));
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM,
                (new_geometry.y + new_geometry.height) -
                (move_resize.window->y + move_resize.window->height));
    } else {
        set_window_size(move_resize.window,
                new_geometry.x,
                new_geometry.y,
                new_geometry.width,
                new_geometry.height);
    }
}

/* FocusIn events are sent when a window received focus. */
static void handle_focus_in(xcb_focus_in_event_t *event)
{
    Window *window;

    /* when a window receives focus because of an grabbing, we assume all is
     * fine and the focus will return to our window soon
     */
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    window = get_window_of_xcb_window(event->event);
    if (window == NULL) {
        return;
    }

    /* someone is trying to steal the focus, stop them from doing that */
    if (window != focus_window) {
        if (focus_window == NULL) {
            xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                    check_window, XCB_CURRENT_TIME);
        } else {
            xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                    focus_window->properties.window, XCB_CURRENT_TIME);
        }
    }
}

/* Unmap notifications are sent after a window decided it wanted to not be seen
 * anymore.
 */
static void handle_unmap_notify(xcb_unmap_notify_event_t *event)
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

/* Map requests are sent when a new window wants to become on screen, this
 * is also where we register new windows and wrap them into the internal
 * Window struct.
 */
static void handle_map_request(xcb_map_request_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        window = create_window(event->window);
    }
    if (window == NULL) {
        return;
    }
    show_window(window);
    set_focus_window(window);
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

/* Property notifications are sent when a window property changes. */
static void handle_property_notify(xcb_property_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }
    cache_window_property(&window->properties, event->atom);
}

/* Configure requests are received when a window wants to choose its own
 * position and size. We allow this for unmanaged windows and floating windows.
 */
static void handle_configure_request(xcb_configure_request_event_t *event)
{
    Window *window;
    int value_index = 0;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL && window->state.mode != WINDOW_MODE_FLOATING) {
        return;
    }

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
    /* ignore border width, stacking etc. */

    xcb_configure_window(connection, event->window, event->value_mask,
            general_values);
}

/* Client messages are sent by a client to our window manager to request certain
 * things.
 */
static void handle_client_message(xcb_client_message_event_t *event)
{
    Window *window;
    int32_t x, y;
    uint32_t width, height;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }

    /* ignore misformatted client messages */
    if (event->format != 32) {
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
        if (window->state.mode != WINDOW_MODE_FLOATING) {
            return;
        }
        if (event->data.data32[2] == _NET_WM_MOVERESIZE_CANCEL) {
            cancel_window_move_resize();
            return;
        }
        initiate_window_move_resize(window, event->data.data32[2],
                event->data.data32[0], event->data.data32[1]);
    /* a window wants to be iconified */
    } else if (event->type == ATOM(WM_CHANGE_STATE)) {
        if (event->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC ||
                event->data.data32[0] == XCB_ICCCM_WM_STATE_WITHDRAWN) {
            hide_window_abruptly(window);
        } else {
            show_window(window);
        }
    /* a window wants to change a window state */
    } else if (event->type == ATOM(_NET_WM_STATE)) {
        if (event->data.data32[1] == ATOM(_NET_WM_STATE_FULLSCREEN)) {
            switch (event->data.data32[0]) {
            /* put the window out of fullscreen */
            case _NET_WM_STATE_REMOVE:
                if (window->state.mode != WINDOW_MODE_FULLSCREEN) {
                    break;
                }
                set_window_mode(window, window->state.previous_mode);
                break;

            /* put the window into fullscreen */
            case _NET_WM_STATE_ADD:
                if (window->state.mode == WINDOW_MODE_FULLSCREEN) {
                    break;
                }
                set_window_mode(window,  WINDOW_MODE_FULLSCREEN);
                break;

            /* toggle the fullscreen state */
            case _NET_WM_STATE_TOGGLE:
                set_window_mode(window,
                        window->state.mode == WINDOW_MODE_FULLSCREEN ?
                        window->state.previous_mode : WINDOW_MODE_FULLSCREEN);
                break;
            }
        }
    }
    /* TODO: handle _NET_REQUEST_FRAME_EXTENTS and _NET_RESTACK_WINDOW */
}

/* Mapping notifications are sent when the modifier keys or keyboard mapping
 * changes.
 */
static void handle_mapping_notify(xcb_mapping_notify_event_t *event)
{
    refresh_keymap(event);
}

/* Screen change notifications are sent when the screen configurations is
 * changed, this can include position, size etc.
 */
static void handle_screen_change(xcb_randr_screen_change_notify_event_t *event)
{
    screen->width_in_pixels = event->width;
    screen->height_in_pixels = event->height;
    screen->width_in_millimeters = event->mwidth;
    screen->height_in_millimeters = event->mheight;
    merge_monitors(query_monitors());
}

/* Handle the given xcb event.
 *
 * Descriptions for each event are above each handler.
 */
void handle_event(xcb_generic_event_t *event)
{
    uint8_t type;

    /* remove the most significant bit, this gets the actual event type */
    type = (event->response_type & ~0x80);

    /* log these events as verbose because they are not helpful mostly */
    if (type == XCB_MOTION_NOTIFY ||
            (type == XCB_CLIENT_MESSAGE &&
             ((xcb_client_message_event_t*) event)->type ==
                ATOM(_NET_WM_USER_TIME))) {
        LOG_VERBOSE("%V\n", event);
    } else {
        LOG("%V\n", event);
    }

    if (type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        handle_screen_change((xcb_randr_screen_change_notify_event_t*) event);
        return;
    }

    switch (type) {
    /* a key was pressed */
    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        /* continue processing keyboard events normally, we need to do this
         * because we use SYNC when grabbing keys/buttons so that we can handle
         * the events ourself but may also decide to replay it to the client it
         * was actually meant for, the replaying is done within the handlers
         */
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

    /* a window took focus */
    case XCB_FOCUS_IN:
        handle_focus_in((xcb_focus_in_event_t*) event);
        break;

    /* a window was destroyed */
    case XCB_DESTROY_NOTIFY:
        handle_destroy_notify((xcb_destroy_notify_event_t*) event);
        break;

    /* a window was removed from the screen */
    case XCB_UNMAP_NOTIFY:
        handle_unmap_notify((xcb_unmap_notify_event_t*) event);
        break;

    /* a window wants to appear on the screen */
    case XCB_MAP_REQUEST:
        handle_map_request((xcb_map_request_event_t*) event);
        break;

    /* a window wants to configure itself */
    case XCB_CONFIGURE_REQUEST:
        handle_configure_request((xcb_configure_request_event_t*) event);
        break;

    /* a window changed a property */
    case XCB_PROPERTY_NOTIFY:
        handle_property_notify((xcb_property_notify_event_t*) event);
        break;

    /* a client sent us a message */
    case XCB_CLIENT_MESSAGE:
        handle_client_message((xcb_client_message_event_t*) event);
        break;

    /* keyboard mapping changed */
    case XCB_MAPPING_NOTIFY:
        handle_mapping_notify((xcb_mapping_notify_event_t*) event);
        break;
    }
}
