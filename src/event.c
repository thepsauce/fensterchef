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
#include "tiling.h"
#include "resources.h"
#include "size_frame.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"

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
volatile sig_atomic_t has_timer_expired;

/* if the user requested to reload the configuration */
bool is_reload_requested;

/* if the client list has changed (if stacking changed, windows were removed or
 * added)
 */
bool has_client_list_changed;

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
    has_timer_expired = true;
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
    return OK;
}

/* Set the client list root property. */
void synchronize_client_list(void)
{
    /* a list of window ids that is synchronized with the actual windows */
    static struct {
        /* the id of the window */
        xcb_window_t *ids;
        /* the number of allocated ids */
        uint32_t length;
    } client_list;

    Window *window;
    uint32_t index = 0;

    if (Window_count > client_list.length) {
        client_list.length = Window_count;
        RESIZE(client_list.ids, client_list.length);
    }

    /* sort the list in order of their age (oldest to newest) */
    for (window = Window_oldest; window != NULL; window = window->newer) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST` property */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_CLIENT_LIST), XCB_ATOM_WINDOW, 32,
            Window_count, client_list.ids);

    index = 0;
    /* sort the list in order of the Z stacking (bottom to top) */
    for (window = Window_bottom; window != NULL; window = window->above) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST_STACKING` property */
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
            ATOM(_NET_CLIENT_LIST_STACKING), XCB_ATOM_WINDOW, 32,
            Window_count, client_list.ids);
}

/* Recursively check if the window is contained within @frame. */
static bool is_window_part_of(Window *window, Frame *frame)
{
    if (frame->left != NULL) {
        return is_window_part_of(window, frame->left) ||
            is_window_part_of(window, frame->right);
    }
    return frame->window == window;
}

/* Synchronize the local data with the X server. */
void synchronize_with_server(void)
{
    xcb_atom_t state_atom;

    /* since the strut of a monitor might have changed because a window with
     * strut got hidden or shown, we need to recompute those
     */
    reconfigure_monitor_frames();

    /* set the border colors of the windows */
    for (Window *window = Window_first; window != NULL; window = window->next) {
        if (window != Window_focus) {
            state_atom = ATOM(_NET_WM_STATE_FOCUSED);
            remove_window_states(window, &state_atom, 1);
        }
        /* set the color of the focused window */
        if (window == Window_focus) {
            window->border_color = configuration.border.focus_color;
        /* deeply set the colors of all windows within the focused frame */
        } else if ((Window_focus == NULL ||
                    Window_focus->state.mode == WINDOW_MODE_TILING) &&
                window->state.mode == WINDOW_MODE_TILING &&
                is_window_part_of(window, Frame_focus)) {
            window->border_color = configuration.border.focus_color;
        /* if the window is the top window or within the focused frame, give it
         * the active color
         */
        } else if (is_window_part_of(window, Frame_focus) ||
                (window->state.mode == WINDOW_MODE_FLOATING &&
                 window == Window_top)) {
            window->border_color = configuration.border.active_color;
        } else {
            window->border_color = configuration.border.color;
        }
    }

    /* configure all visible windows and map them */
    for (Window *window = Window_top; window != NULL; window = window->below) {
        if (!window->state.is_visible) {
            continue;
        }
        configure_client(&window->client, window->x, window->y,
                window->width, window->height, window->border_size);
        change_client_attributes(&window->client,
                window->client.background_color, window->border_color);
        state_atom = ATOM(_NET_WM_STATE_HIDDEN);
        remove_window_states(window, &state_atom, 1);
        map_client(&window->client);
    }

    /* unmap all invisible windows */
    for (Window *window = Window_bottom; window != NULL;
            window = window->above) {
        if (!window->state.is_visible) {
            state_atom = ATOM(_NET_WM_STATE_HIDDEN);
            add_window_states(window, &state_atom, 1);
            unmap_client(&window->client);
        }
    }
}

/* Run the next cycle of the event loop. */
int next_cycle(void)
{
    int connection_error;
    Window *old_focus_window;
    Frame *old_focus_frame;
    xcb_generic_event_t *event;
    fd_set set;

    connection_error = xcb_connection_has_error(connection);
    if (!Fensterchef_is_running || connection_error > 0) {
        if (connection_error > 0) {
            LOG_ERROR("connection error: %X\n", connection_error);
        }
        return ERROR;
    }

    /* save the old focus for comparison (checking if the focus changed) */
    old_focus_window = Window_focus;
    old_focus_frame = Frame_focus;
    /* make sure the pointers do not get freed */
    if (old_focus_window != NULL) {
        reference_window(old_focus_window);
    }
    if (old_focus_frame != NULL) {
        reference_frame(old_focus_frame);
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
            handle_window_list_event(event);

            handle_event(event);

            if (is_reload_requested) {
                reload_user_configuration();
                is_reload_requested = false;
            }

            free(event);
        }

        synchronize_with_server();
        /* update the client list properties */
        if (has_client_list_changed) {
            synchronize_client_list();
            has_client_list_changed = false;
        }

        if (old_focus_window != Window_focus) {
            set_input_focus(Window_focus);
        }

        if (old_focus_frame != Frame_focus ||
                (Frame_focus->window == Window_focus &&
                 old_focus_window != Window_focus)) {
            /* indicate the focused frame if there is no window inside or there
             * is no border to indicate that the frame is focused
             */
            if (Frame_focus->window == NULL ||
                     Frame_focus->window->border_size == 0) {
                char number[MAXIMUM_DIGITS(Frame_focus->number) + 1];

                snprintf(number, sizeof(number), "%" PRIu32,
                        Frame_focus->number);
                set_notification((utf8_t*) (Frame_focus->number > 0 ? number :
                            Frame_focus->left == NULL ?  "Current frame" :
                            "Current frames"),
                        Frame_focus->x + Frame_focus->width / 2,
                        Frame_focus->y + Frame_focus->height / 2);
            }
            LOG("frame %F was focused\n", Frame_focus);
        }
    }

    if (has_timer_expired) {
        unmap_client(&notification);
        has_timer_expired = false;
    }

    /* no longer need them */
    if (old_focus_frame != NULL) {
        dereference_frame(old_focus_frame);
    }
    if (old_focus_window != NULL) {
        dereference_window(old_focus_window);
    }

    /* flush after every series of events so all changes are reflected */
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
    core_cursor_t cursor;

    /* check if no window is already being moved/resized */
    if (move_resize.window != NULL) {
        return;
    }

    /* only allow sizing/moving of floating and tiling windows */
    if (window->state.mode != WINDOW_MODE_FLOATING &&
            window->state.mode != WINDOW_MODE_TILING) {
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

    /* determine a fitting cursor */
    switch (direction) {
    case _NET_WM_MOVERESIZE_MOVE:
        cursor = configuration.general.moving_cursor;
        break;

    case _NET_WM_MOVERESIZE_SIZE_LEFT:
    case _NET_WM_MOVERESIZE_SIZE_RIGHT:
        cursor = configuration.general.horizontal_cursor;
        break;

    case _NET_WM_MOVERESIZE_SIZE_TOP:
    case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
        cursor = configuration.general.vertical_cursor;
        break;

    default:
        cursor = configuration.general.sizing_cursor;
    }

    const xcb_cursor_t xcb_cursor = load_cursor(cursor);
    /* grab mouse events, we will then receive all mouse events */
    grab_cookie = xcb_grab_pointer(connection, false, screen->root,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root,
            xcb_cursor, XCB_CURRENT_TIME);
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

    /* restore the old position and size as good as we can */
    frame = get_frame_of_window(move_resize.window);
    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT,
                move_resize.window->x - move_resize.initial_geometry.x);
        bump_frame_edge(frame, FRAME_EDGE_TOP,
                move_resize.window->y - move_resize.initial_geometry.y);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT,
                (move_resize.initial_geometry.x +
                 move_resize.initial_geometry.width) -
                    (move_resize.window->x + move_resize.window->width));
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM,
                (move_resize.initial_geometry.y +
                 move_resize.initial_geometry.height) -
                    (move_resize.window->y + move_resize.window->height));
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

    key = find_configured_key_by_code(&configuration, event->state,
            event->detail, 0);
    if (key != NULL) {
        /* before a key binding, hide the notification window */
        alarm(0);
        unmap_client(&notification);

        LOG("performing action(s): %A\n", key->number_of_actions,
                key->actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
            do_action(&key->actions[i], Window_focus);
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

    key = find_configured_key_by_code(&configuration, event->state,
            event->detail, BINDING_FLAG_RELEASE);
    if (key != NULL) {
        LOG("performing action(s): %A\n", key->number_of_actions,
                key->actions);
        for (uint32_t i = 0; i < key->number_of_actions; i++) {
            do_action(&key->actions[i], Window_focus);
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
        /* use this event only for stopping the resize */
        return;
    }

    window = get_window_of_xcb_window(event->event);
    /* allow window to be NULL */

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

    if (move_resize.window != NULL) {
        /* release mouse events back to the applications */
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        move_resize.window = NULL;
        /* use this event only for finishing the resize */
        return;
    }

    window = get_window_of_xcb_window(event->event);
    /* allow window to be NULL */

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
    Size minimum, maximum;
    int32_t delta_x, delta_y;
    int32_t left_delta, top_delta, right_delta, bottom_delta;
    Frame *frame;

    if (move_resize.window == NULL) {
        LOG_ERROR("receiving motion events without a window to move?\n");
        return;
    }

    new_geometry = move_resize.initial_geometry;

    get_minimum_window_size(move_resize.window, &minimum);
    get_maximum_window_size(move_resize.window, &maximum);

    delta_x = move_resize.start.x - event->root_x;
    delta_y = move_resize.start.y - event->root_y;

    /* prevent overflows and clip so that moving an edge when no more size is
     * available does not move the window
     */
    if (delta_x < 0) {
        left_delta = -MIN((uint32_t) -delta_x,
                new_geometry.width - minimum.width);
    } else {
        left_delta = MIN((uint32_t) delta_x,
                maximum.width - new_geometry.width);
    }
    if (delta_y < 0) {
        top_delta = -MIN((uint32_t) -delta_y,
                new_geometry.height - minimum.height);
    } else {
        top_delta = MIN((uint32_t) delta_y,
                maximum.height - new_geometry.height);
    }

    /* prevent overflows */
    if (delta_x > 0) {
        right_delta = MIN((uint32_t) delta_x, new_geometry.width);
    } else {
        right_delta = delta_x;
    }
    if (delta_y > 0) {
        bottom_delta = MIN((uint32_t) delta_y, new_geometry.height);
    } else {
        bottom_delta = delta_y;
    }

    switch (move_resize.direction) {
    /* the top left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
        new_geometry.x -= left_delta;
        new_geometry.width += left_delta;
        new_geometry.y -= top_delta;
        new_geometry.height += top_delta;
        break;

    /* the top size of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOP:
        new_geometry.y -= top_delta;
        new_geometry.height += top_delta;
        break;

    /* the top right corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
        new_geometry.width -= right_delta;
        new_geometry.y -= top_delta;
        new_geometry.height += top_delta;
        break;

    /* the right side of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_RIGHT:
        new_geometry.width -= right_delta;
        break;

    /* the bottom right corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
        new_geometry.width -= right_delta;
        new_geometry.height -= bottom_delta;
        break;

    /* the bottom side of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
        new_geometry.height -= bottom_delta;
        break;

    /* the bottom left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
        new_geometry.x -= left_delta;
        new_geometry.width += left_delta;
        new_geometry.height -= bottom_delta;
        break;

    /* the bottom left corner of the window is being moved */
    case _NET_WM_MOVERESIZE_SIZE_LEFT:
        new_geometry.x -= left_delta;
        new_geometry.width += left_delta;
        break;

    /* the entire window is being moved */
    case _NET_WM_MOVERESIZE_MOVE:
        new_geometry.x -= delta_x;
        new_geometry.y -= delta_y;
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

    window->client.is_mapped = false;

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
    bool is_first_time = false;
    struct configuration_association association = { .number_of_actions = 0 };

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        window = create_window(event->window, &association);
        is_first_time = true;
    }
    if (window == NULL) {
        LOG("not managing %w\n", event->window);
        return;
    }

    /* check if the user defined actions to run when this window receives its
     * number
     */
    if (association.number_of_actions > 0) {
        LOG("running associated actions: %A\n", association.number_of_actions,
                association.actions);
        for (uint32_t j = 0; j < association.number_of_actions; j++) {
            do_action(&association.actions[j], window);
        }
    } else {
        /* if a window does not start in normal state, do not map it */
        if (is_first_time && (window->hints.flags & XCB_ICCCM_WM_HINT_STATE) &&
                window->hints.initial_state != XCB_ICCCM_WM_STATE_NORMAL) {
            LOG("window %W starts off as hidden window\n", window);
            return;
        }

        show_window(window);
        if (is_window_focusable(window)) {
            set_focus_window_with_frame(window);
        }
    }
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

    /* reload the resources if they changed */
    if (event->window == screen->root) {
        if (event->atom == XCB_ATOM_RESOURCE_MANAGER) {
            reload_resources();
        }
        return;
    }

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        return;
    }
    cache_window_property(window, event->atom);
}

/* Configure requests are received when a window wants to choose its own
 * position and size. We allow this for unmanaged windows.
 */
static void handle_configure_request(xcb_configure_request_event_t *event)
{
    Window *window;
    int value_index = 0;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
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
    if ((event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
        general_values[value_index++] = event->border_width;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_SIBLING)) {
        general_values[value_index++] = event->sibling;
    }
    if ((event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)) {
        general_values[value_index++] = event->stack_mode;
    }

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
                get_monitor_from_rectangle_or_primary(x, y, width, height),
                &x, &y, width, height, event->data.data32[0]);
        set_window_size(window, x, y, width, height);
    /* request to dynamically move and resize the window */
    } else if (event->type == ATOM(_NET_WM_MOVERESIZE)) {
        if (event->data.data32[2] == _NET_WM_MOVERESIZE_CANCEL) {
            cancel_window_move_resize();
            return;
        }
        initiate_window_move_resize(window, event->data.data32[2],
                event->data.data32[0], event->data.data32[1]);
    /* a window wants to be iconified or put into normal state (shown) */
    } else if (event->type == ATOM(WM_CHANGE_STATE)) {
        switch (event->data.data32[0]) {
        /* hide the window */
        case XCB_ICCCM_WM_STATE_ICONIC:
        case XCB_ICCCM_WM_STATE_WITHDRAWN:
            hide_window(window);
            break;

        /* make the window normal again (show it) */
        case XCB_ICCCM_WM_STATE_NORMAL:
            show_window(window);
            break;
        }
    /* a window wants to change a window state */
    } else if (event->type == ATOM(_NET_WM_STATE)) {
        if (event->data.data32[1] == ATOM(_NET_WM_STATE_FULLSCREEN) ||
                event->data.data32[1] == ATOM(_NET_WM_STATE_MAXIMIZED_HORZ) ||
                event->data.data32[1] == ATOM(_NET_WM_STATE_MAXIMIZED_VERT)) {
            switch (event->data.data32[0]) {
            /* put the window out of fullscreen */
            case _NET_WM_STATE_REMOVE:
                set_window_mode(window, window->state.previous_mode);
                break;

            /* put the window into fullscreen */
            case _NET_WM_STATE_ADD:
                set_window_mode(window, WINDOW_MODE_FULLSCREEN);
                break;

            /* toggle the fullscreen state */
            case _NET_WM_STATE_TOGGLE:
                set_window_mode(window,
                        window->state.mode == WINDOW_MODE_FULLSCREEN ?
                        (window->state.previous_mode == WINDOW_MODE_FULLSCREEN ?
                            WINDOW_MODE_FLOATING : window->state.previous_mode) :
                        WINDOW_MODE_FULLSCREEN);
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

    /* log these events as verbose because they are not helpful */
    if (type == XCB_MOTION_NOTIFY ||
            (type == XCB_CLIENT_MESSAGE &&
             ((xcb_client_message_event_t*) event)->type ==
                ATOM(_NET_WM_USER_TIME))) {
        LOG_VERBOSE("%V\n", event);
    } else {
        LOG("%V\n", event);
    }

    if (randr_event_base > 0 &&
            type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
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
