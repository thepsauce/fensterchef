#include <signal.h>

#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <X11/XKBlib.h>

#include <X11/extensions/Xrandr.h>

#include "configuration/configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "frame_sizing.h"
#include "log.h"
#include "monitor.h"
#include "notification.h"
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

/* first index of an xkb event */
int xkb_event_base, xkb_error_base;

/* first index of a randr event */
int randr_event_base, randr_error_base;

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
    FcWindow *window;
    /* how to move or resize the window */
    wm_move_resize_direction_t direction;
    /* initial position and size of the window */
    Rectangle initial_geometry;
    /* the initial position of the mouse */
    Point start;
} move_resize;

/* Handle an incoming alarm. */
static void alarm_handler(int signal_number)
{
    has_timer_expired = true;
    /* make sure the signal handler sticks around and is not deinstalled */
    signal(signal_number, alarm_handler);
}

/* Create a signal handler for `SIGALRM`. */
void initialize_signal_handlers(void)
{
    signal(SIGALRM, alarm_handler);
}

/* Set the client list root property. */
void synchronize_client_list(void)
{
    /* a list of window ids that is synchronized with the actual windows */
    static struct {
        /* the id of the window */
        Window *ids;
        /* the number of allocated ids */
        unsigned length;
    } client_list;

    Window root;
    FcWindow *window;
    unsigned index = 0;

    root = DefaultRootWindow(display);

    /* allocate more ids if needed */
    if (Window_count > client_list.length) {
        client_list.length = Window_count;
        REALLOCATE(client_list.ids, client_list.length);
    }

    /* sort the list in order of their age (oldest to newest) */
    for (window = Window_oldest; window != NULL; window = window->newer) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST` property */
    XChangeProperty(display, root, ATOM(_NET_CLIENT_LIST), XA_WINDOW, 32,
            PropModeReplace, (unsigned char*) client_list.ids, Window_count);

    index = 0;
    /* sort the list in order of the Z stacking (bottom to top) */
    for (window = Window_bottom; window != NULL; window = window->above) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST_STACKING` property */
    XChangeProperty(display, root, ATOM(_NET_CLIENT_LIST_STACKING), XA_WINDOW,
            32, PropModeReplace,
            (unsigned char*) client_list.ids, Window_count);
}

/* Recursively check if the window is contained within @frame. */
static bool is_window_part_of(FcWindow *window, Frame *frame)
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
    Atom atoms[2];

    /* since the strut of a monitor might have changed because a window with
     * strut got hidden or shown, we need to recompute those
     */
    reconfigure_monitor_frames();

    /* set the border colors of the windows and manage the focused state */
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
        /* remove the focused state from windows that are not focused */
        if (window != Window_focus) {
            atoms[0] = ATOM(_NET_WM_STATE_FOCUSED);
            remove_window_states(window, atoms, 1);
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
    for (FcWindow *window = Window_top;
            window != NULL;
            window = window->below) {
        if (!window->state.is_visible) {
            continue;
        }
        configure_client(&window->client, window->x, window->y,
                window->width, window->height, window->border_size);
        change_client_attributes(&window->client,
                window->client.background, window->border_color);

        atoms[0] = ATOM(_NET_WM_STATE_HIDDEN);
        remove_window_states(window, atoms, 1);

        if (window->wm_state != NormalState) {
            window->wm_state = NormalState;
            atoms[0] = NormalState;
            /* no icon */
            atoms[1] = None;
            XChangeProperty(display, window->client.id, ATOM(WM_STATE),
                    ATOM(WM_STATE), 32, PropModeReplace,
                    (unsigned char*) atoms, 2);
        }

        map_client(&window->client);
    }

    /* unmap all invisible windows */
    for (FcWindow *window = Window_bottom;
            window != NULL;
            window = window->above) {
        if (window->state.is_visible) {
            continue;
        }

        atoms[0] = ATOM(_NET_WM_STATE_HIDDEN);
        add_window_states(window, atoms, 1);

        if (window->wm_state != WithdrawnState) {
            window->wm_state = WithdrawnState;
            atoms[0] = WithdrawnState;
            /* no icon */
            atoms[1] = None;
            XChangeProperty(display, window->client.id, ATOM(WM_STATE),
                    ATOM(WM_STATE), 32, PropModeReplace,
                    (unsigned char*) atoms, 2);
        }

        unmap_client(&window->client);
    }
}

/* Run the next cycle of the event loop. */
int next_cycle(void)
{
    FcWindow *old_focus_window;
    Frame *old_focus_frame;
    XEvent event;
    fd_set set;

    /* signal to stop running */
    if (!Fensterchef_is_running) {
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
     * descriptor for the X display arrives; when a signal is received,
     * `select()` will however also unblock and return -1
     */
    if (select(x_file_descriptor + 1, &set, NULL, NULL, NULL) > 0) {
        /* handle all received events */
        while (XPending(display)) {
            XNextEvent(display, &event);

            handle_window_list_event(&event);

            handle_event(&event);

            if (is_reload_requested) {
                reload_user_configuration();
                is_reload_requested = false;
            }
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
            /* indicate the focused frame if one of:
             * - the frame has no inner window
             * - the inner window has no border
             * - the focused window is a non tiling window
             */
            if (Frame_focus->window == NULL ||
                     Frame_focus->window->border_size == 0 ||
                     (Window_focus != NULL &&
                        Window_focus->state.mode != WINDOW_MODE_TILING)) {
                char number[MAXIMUM_DIGITS(Frame_focus->number) + 1];

                snprintf(number, sizeof(number), "%u",
                        Frame_focus->number);
                set_system_notification(Frame_focus->number > 0 ? number :
                            Frame_focus->left == NULL ?  "Current frame" :
                            "Current frames",
                        Frame_focus->x + Frame_focus->width / 2,
                        Frame_focus->y + Frame_focus->height / 2);
            }
            LOG("frame %F was focused\n", Frame_focus);
        }
    }

    if (has_timer_expired) {
        unmap_client(&system_notification->client);
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
    XFlush(display);

    return OK;
}

/* Start moving/resizing given window. */
void initiate_window_move_resize(FcWindow *window,
        wm_move_resize_direction_t direction,
        int start_x, int start_y)
{
    Window root;
    const char *cursor_name;

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

    root = DefaultRootWindow(display);

    /* get the mouse position if the caller does not supply it */
    if (start_x < 0) {
        Window other_root;
        Window child;
        int window_x;
        int window_y;
        unsigned int mask;

        XQueryPointer(display, root, &other_root, &child,
                &start_x, &start_y, /* root_x, root_y */
                &window_x, &window_y, &mask);
    }

    /* figure out a good direction if the caller does not supply one */
    if (direction == _NET_WM_MOVERESIZE_AUTO) {
        const unsigned border = 2 * configuration.border.size;
        /* check if the mouse is at the top */
        if (start_y < window->y + configuration.mouse.resize_tolerance) {
            if (start_x < window->x + configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
            } else if (start_x - (int) (border + window->width) >=
                    window->x - configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
            } else {
                direction = _NET_WM_MOVERESIZE_SIZE_TOP;
            }
        /* check if the mouse is at the bottom */
        } else if (start_y - (int) (border + window->height) >=
                window->y - configuration.mouse.resize_tolerance) {
            if (start_x < window->x + configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
            } else if (start_x - (int) (border + window->width) >=
                    window->x - configuration.mouse.resize_tolerance) {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
            } else {
                direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
            }
        /* check if the mouse is left */
        } else if (start_x < window->x + configuration.mouse.resize_tolerance) {
            direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
        /* check if the mouse is right */
        } else if (start_x - (int) (border + window->width) >=
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
        cursor_name = configuration.general.moving_cursor;
        break;

    case _NET_WM_MOVERESIZE_SIZE_LEFT:
    case _NET_WM_MOVERESIZE_SIZE_RIGHT:
        cursor_name = configuration.general.horizontal_cursor;
        break;

    case _NET_WM_MOVERESIZE_SIZE_TOP:
    case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
        cursor_name = configuration.general.vertical_cursor;
        break;

    default:
        cursor_name = configuration.general.sizing_cursor;
    }

    const Cursor cursor = load_cursor(cursor_name);
    /* grab mouse events, we will then receive all mouse events */
    XGrabPointer(display, root, False,
            ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
            GrabModeAsync, GrabModeAsync, root, cursor, CurrentTime);
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
    XUngrabPointer(display, CurrentTime);

    move_resize.window = NULL;
}

/* Key press events are sent when a grabbed key is pressed. */
static void handle_key_press(XKeyPressedEvent *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state, event->keycode, 0);
    if (key != NULL) {
        if (system_notification != NULL) {
            /* before a key binding, hide the notification window */
            alarm(0);
            unmap_client(&system_notification->client);
        }

        LOG("evaluating expression: %A\n",
                &key->expression);
        evaluate_expression(&key->expression, NULL);
    }
}

/* Key release events are sent when a grabbed key is released. */
static void handle_key_release(XKeyReleasedEvent *event)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, event->state,
            event->keycode, BINDING_FLAG_RELEASE);
    if (key != NULL) {
        LOG("evaluating expression: %A\n",
                &key->expression);
        evaluate_expression(&key->expression, NULL);
    }
}

/* Button press events are sent when a grabbed button is pressed. */
static void handle_button_press(XButtonPressedEvent *event)
{
    struct configuration_button *button;

    if (move_resize.window != NULL) {
        cancel_window_move_resize();
        /* use this event only for stopping the resize */
        return;
    }

    /* set the pressed window so actions can use it */
    Window_pressed = get_fensterchef_window(event->window);

    button = find_configured_button(&configuration, event->state,
            event->button, 0);
    if (button != NULL) {
        LOG("evaluating expression: %A\n",
                &button->expression);
        evaluate_expression(&button->expression, NULL);

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            XAllowEvents(display, ReplayPointer, event->time);
        }
    }

    Window_pressed = NULL;
}

/* Button releases are sent when a grabbed button is released. */
static void handle_button_release(XButtonReleasedEvent *event)
{
    struct configuration_button *button;

    if (move_resize.window != NULL) {
        /* release mouse events back to the applications */
        XUngrabPointer(display, CurrentTime);

        move_resize.window = NULL;
        /* use this event only for finishing the resize */
        return;
    }

    /* set the pressed window so actions can use it */
    Window_pressed = get_fensterchef_window(event->window);

    button = find_configured_button(&configuration, event->state,
            event->button, BINDING_FLAG_RELEASE);
    if (button != NULL) {
        LOG("evaluating expression: %A\n",
                &button->expression);
        evaluate_expression(&button->expression, NULL);

        /* make the event pass through to the underlying window */
        if ((button->flags & BINDING_FLAG_TRANSPARENT)) {
            XAllowEvents(display, ReplayPointer, event->time);
        }
    }

    Window_pressed = NULL;
}

/* Motion notifications (mouse move events) are only sent when we grabbed them.
 * This only happens when a floating window is being moved.
 */
static void handle_motion_notify(XMotionEvent *event)
{
    Rectangle new_geometry;
    Size minimum, maximum;
    int delta_x, delta_y;
    int left_delta, top_delta, right_delta, bottom_delta;
    Frame *frame;

    if (move_resize.window == NULL) {
        LOG_ERROR("receiving motion events without a window to move?\n");
        return;
    }

    new_geometry = move_resize.initial_geometry;

    get_minimum_window_size(move_resize.window, &minimum);
    get_maximum_window_size(move_resize.window, &maximum);

    delta_x = move_resize.start.x - event->x_root;
    delta_y = move_resize.start.y - event->y_root;

    /* prevent overflows and clip so that moving an edge when no more size is
     * available does not move the window
     */
    if (delta_x < 0) {
        left_delta = -MIN((unsigned) -delta_x,
                new_geometry.width - minimum.width);
    } else {
        left_delta = MIN((unsigned) delta_x,
                maximum.width - new_geometry.width);
    }
    if (delta_y < 0) {
        top_delta = -MIN((unsigned) -delta_y,
                new_geometry.height - minimum.height);
    } else {
        top_delta = MIN((unsigned) delta_y,
                maximum.height - new_geometry.height);
    }

    /* prevent overflows */
    if (delta_x > 0) {
        right_delta = MIN((unsigned) delta_x, new_geometry.width);
    } else {
        right_delta = delta_x;
    }
    if (delta_y > 0) {
        bottom_delta = MIN((unsigned) delta_y, new_geometry.height);
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
static void handle_unmap_notify(XUnmapEvent *event)
{
    FcWindow *window;

    window = get_fensterchef_window(event->window);
    if (window == NULL) {
        return;
    }

    window->client.is_mapped = false;

    /* if the currently moved window is unmapped */
    if (window == move_resize.window) {
        /* release mouse events back to the applications */
        XUngrabPointer(display, CurrentTime);

        move_resize.window = NULL;
    }

    hide_window(window);
}

/* Map requests are sent when a new window wants to become on screen, this
 * is also where we register new windows and wrap them into the internal
 * `FcWindow` struct.
 */
static void handle_map_request(XMapRequestEvent *event)
{
    FcWindow *window;
    bool is_first_time = false;
    struct configuration_association association = {
        .expression.instruction_size = 0
    };

    window = get_fensterchef_window(event->window);
    if (window == NULL) {
        window = create_window(event->window, &association);
        is_first_time = true;
    }
    if (window == NULL) {
        LOG("not managing %w\n", event->window);
        return;
    }

    /* check if the user defined an expression to run when after the window
     * is mapped
     */
    if (association.expression.instruction_size > 0) {
        LOG("running associated expression: %A\n", &association.expression);
        Window_pressed = window;
        evaluate_expression(&association.expression, NULL);
        Window_pressed = NULL;
    } else {
        /* if a window does not start in normal state, do not map it */
        if (is_first_time && (window->hints.flags & StateHint) &&
                window->hints.initial_state != NormalState) {
            LOG("window %W starts off as hidden window\n", window);
            return;
        }

        show_window(window);
        update_window_layer(window);
        if (is_window_focusable(window)) {
            set_focus_window_with_frame(window);
        }
    }
}

/* Destroy notifications are sent when a window leaves the X server.
 * Good bye to that window!
 */
static void handle_destroy_notify(XDestroyWindowEvent *event)
{
    FcWindow *window;

    window = get_fensterchef_window(event->window);
    if (window != NULL) {
        destroy_window(window);
    }
}

/* Property notifications are sent when a window property changes. */
static void handle_property_notify(XPropertyEvent *event)
{
    FcWindow *window;

    window = get_fensterchef_window(event->window);
    if (window == NULL) {
        return;
    }
    cache_window_property(window, event->atom);
}

/* Configure requests are received when a window wants to choose its own
 * position and size. We allow this for unmanaged windows.
 */
static void handle_configure_request(XConfigureRequestEvent *event)
{
    XWindowChanges changes;
    FcWindow *window;

    window = get_fensterchef_window(event->window);
    if (window != NULL) {
        XEvent notify_event;

        /* fake a configure notify, this is needed for many windows to work,
         * otherwise they get in a bugged state which could have various reasons
         * this all because we are a *tiling* window manager
         */
        memset(&notify_event, 0, sizeof(notify_event));
        notify_event.type = ConfigureNotify;
        notify_event.xconfigure.event = window->client.id;
        notify_event.xconfigure.window = window->client.id;
        notify_event.xconfigure.x = window->x;
        notify_event.xconfigure.y = window->y;
        notify_event.xconfigure.width = window->width;
        notify_event.xconfigure.height = window->height;
        notify_event.xconfigure.border_width = is_window_borderless(window) ?
            0 : window->border_size;
        LOG("sending fake configure notify event %V to %W\n",
                &notify_event, window);
        XSendEvent(display, window->client.id, false,
                StructureNotifyMask, &notify_event);
        return;
    }

    if ((event->value_mask & CWX)) {
        changes.x = event->x;
    }
    if ((event->value_mask & CWY)) {
        changes.y = event->y;
    }
    if ((event->value_mask & CWWidth)) {
        changes.width = event->width;
    }
    if ((event->value_mask & CWHeight)) {
        changes.height = event->height;
    }
    if ((event->value_mask & CWBorderWidth)) {
        changes.border_width = event->border_width;
    }
    if ((event->value_mask & CWSibling)) {
        changes.sibling = event->above;
    }
    if ((event->value_mask & CWStackMode)) {
        changes.stack_mode = event->detail;
    }

    LOG("configuring unmanaged window %w\n",
            event->window);
    XConfigureWindow(display, event->window, event->value_mask, &changes);
}

/* Client messages are sent by a client to our window manager to request certain
 * things.
 */
static void handle_client_message(XClientMessageEvent *event)
{
    FcWindow *window;
    int x, y;
    unsigned width, height;

    window = get_fensterchef_window(event->window);
    if (window == NULL) {
        return;
    }

    /* ignore misformatted client messages */
    if (event->format != 32) {
        return;
    }

    /* request to close the window */
    if (event->message_type == ATOM(_NET_CLOSE_WINDOW)) {
        close_window(window);
    /* request to move and resize the window */
    } else if (event->message_type == ATOM(_NET_MOVERESIZE_WINDOW)) {
        x = event->data.l[1];
        y = event->data.l[2];
        width = event->data.l[3];
        height = event->data.l[4];
        adjust_for_window_gravity(
                get_monitor_from_rectangle_or_primary(x, y, width, height),
                &x, &y, width, height, event->data.l[0]);
        set_window_size(window, x, y, width, height);
    /* request to dynamically move and resize the window */
    } else if (event->message_type == ATOM(_NET_WM_MOVERESIZE)) {
        if (event->data.l[2] == _NET_WM_MOVERESIZE_CANCEL) {
            cancel_window_move_resize();
            return;
        }
        initiate_window_move_resize(window, event->data.l[2],
                event->data.l[0], event->data.l[1]);
    /* a window wants to be iconified or put into normal state (shown) */
    } else if (event->message_type == ATOM(WM_CHANGE_STATE)) {
        switch (event->data.l[0]) {
        /* hide the window */
        case IconicState:
        case WithdrawnState:
            hide_window(window);
            break;

        /* make the window normal again (show it) */
        case NormalState:
            show_window(window);
            break;
        }
    /* a window wants to change a window state */
    } else if (event->message_type == ATOM(_NET_WM_STATE)) {
        const Atom atom = event->data.l[1];
        if (atom == ATOM(_NET_WM_STATE_ABOVE)) {
            switch (event->data.l[0]) {
            /* only react to adding */
            case _NET_WM_STATE_ADD:
                update_window_layer(window);
                break;
            }
        } else if (atom == ATOM(_NET_WM_STATE_FULLSCREEN) ||
                atom == ATOM(_NET_WM_STATE_MAXIMIZED_HORZ) ||
                atom == ATOM(_NET_WM_STATE_MAXIMIZED_VERT)) {
            switch (event->data.l[0]) {
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
                            WINDOW_MODE_FLOATING :
                            window->state.previous_mode) :
                        WINDOW_MODE_FULLSCREEN);
                break;
            }
        }
    }
    /* TODO: handle _NET_REQUEST_FRAME_EXTENTS and _NET_RESTACK_WINDOW */
}

/* Handle the given X event.
 *
 * Descriptions for each event are above each handler.
 */
void handle_event(XEvent *event)
{
    if (event->type == xkb_event_base) {
        LOG("%V\n", event);
        switch (((XkbAnyEvent*) event)->xkb_type) {
        /* the keyboard mapping changed */
        case XkbNewKeyboardNotify:
        case XkbMapNotify:
            XkbRefreshKeyboardMapping((XkbMapNotifyEvent*) event);
            grab_configured_keys();
            break;
        }
        return;
    }

    if (event->type == randr_event_base) {
        LOG("%V\n", event);
        /* screen change notifications are sent when the screen configuration is
         * changed, this can include position, size etc.
         */
        merge_monitors(query_monitors());
        return;
    }

    /* log these events as verbose because they are not helpful */
    if (event->type == MotionNotify || (event->type == ClientMessage &&
                event->xclient.message_type == ATOM(_NET_WM_USER_TIME))) {
        LOG_VERBOSE("%V\n", event);
    } else {
        LOG("%V\n", event);
    }

    switch (event->type) {
    /* a key was pressed */
    case KeyPress:
        handle_key_press(&event->xkey);
        break;

    /* a key was released */
    case KeyRelease:
        handle_key_release(&event->xkey);
        break;

    /* a mouse button was pressed */
    case ButtonPress:
        handle_button_press(&event->xbutton);
        /* continue processing pointer events normally, we need to do this
         * because we use SYNC when grabbing buttons so that we can handle
         * the events ourself but may also decide to replay it to the client it
         * was actually meant for, the replaying is done within the handlers
         */
        XAllowEvents(display, AsyncPointer, event->xbutton.time);
        break;

    /* a mouse button was released */
    case ButtonRelease:
        handle_button_release(&event->xbutton);
        /* continue processing pointer events normally */
        XAllowEvents(display, AsyncPointer, event->xbutton.time);
        break;

    /* the mouse was moved */
    case MotionNotify:
        handle_motion_notify(&event->xmotion);
        break;

    /* a window was destroyed */
    case DestroyNotify:
        handle_destroy_notify(&event->xdestroywindow);
        break;

    /* a window was removed from the screen */
    case UnmapNotify:
        handle_unmap_notify(&event->xunmap);
        break;

    /* a window wants to appear on the screen */
    case MapRequest:
        handle_map_request(&event->xmaprequest);
        break;

    /* a window wants to configure itself */
    case ConfigureRequest:
        handle_configure_request(&event->xconfigurerequest);
        break;

    /* a window changed a property */
    case PropertyNotify:
        handle_property_notify(&event->xproperty);
        break;

    /* a client sent us a message */
    case ClientMessage:
        handle_client_message(&event->xclient);
        break;
    }
}
