#include <inttypes.h>

#include "action.h"
#include "fensterchef.h"
#include "log.h"

/* This file handles all kinds of xcb events.
 *
 * Note the difference between REQUESTS and NOTIFICATIONS.
 * REQUEST: What is requested has not happened yet and will not happen
 * until the window manager does something.
 *
 * NOTIFICATIONS: What is notified ALREADY happened, there is nothing
 * to do now but to take note of it.
 */

/* this is used for moving a popup window */
static struct {
    /* the initial position of the moved window (for ESCAPE cancellation) */
    int16_t start_x;
    int16_t start_y;
    /* previous mouse position, needed to compute the mouse motion */
    int16_t old_mouse_x;
    int16_t old_mouse_y;
    /* the window that is being moved */
    xcb_window_t xcb_window;
} selected_window;

/* Map requests are sent when a new window wants to become on screen, this
 * is also where we register new windows and wrap them into the internal
 * Window struct.
 */
static void handle_map_request(xcb_map_request_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
        return;
    }
    window = create_window(event->window);
    set_focus_window(window);
}

/* Button press events are sent when the mouse is pressed together with
 * the special modifier key. This is used to move popup windows.
 */
static void handle_button_press(xcb_button_press_event_t *event)
{
    xcb_get_geometry_reply_t    *geometry;
    Window                      *window;
    xcb_window_t                root;

    window = get_window_of_xcb_window(event->child);
    if (window == NULL || window->state != WINDOW_STATE_POPUP) {
        return;
    }

    geometry = xcb_get_geometry_reply(g_dpy,
            xcb_get_geometry(g_dpy, event->child), NULL);
    if (geometry == NULL) {
        return;
    }
    selected_window.start_x = event->root_y;
    selected_window.start_y = event->root_y;

    selected_window.old_mouse_x = event->root_x;
    selected_window.old_mouse_y = event->root_y;

    selected_window.xcb_window = event->child;

    root = SCREEN(g_screen_no)->root;
    xcb_grab_pointer(g_dpy, 0, root,
            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root,
            XCB_NONE, XCB_CURRENT_TIME);
}

/* Motion notifications (mouse move events) are only sent when we grabbed them.
 * This only happens when a popup window is being moved.
 */
static void handle_motion_notify(xcb_motion_notify_event_t *event)
{
    xcb_get_geometry_reply_t        *geometry;

    geometry = xcb_get_geometry_reply(g_dpy,
            xcb_get_geometry(g_dpy, selected_window.xcb_window), NULL);
    if (geometry == NULL) {
        xcb_ungrab_pointer(g_dpy, XCB_CURRENT_TIME);
        return;
    }
    g_values[0] = geometry->x + (event->root_x - selected_window.old_mouse_x);
    g_values[1] = geometry->y + (event->root_y - selected_window.old_mouse_y);
    selected_window.old_mouse_x = event->root_x;
    selected_window.old_mouse_y = event->root_y;
    xcb_configure_window(g_dpy, selected_window.xcb_window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, g_values);
}

/* Button releases are only sent when we grabbed them. This only happens
 * when a popup window is being moved.
 */
static void handle_button_release(xcb_button_release_event_t *event)
{
    (void) event;
    xcb_ungrab_pointer(g_dpy, XCB_CURRENT_TIME);
}

/* Property notifications are sent when a window atom changes, this can
 * be many atoms, but the main ones handled are WM_NAME, WM_SIZE_HINTS.
 *
 * TODO: make special handling for WM_NORMAL_HINTS just like with WM_NAME
 * for fullscreen.
 */
static void handle_property_notify(xcb_property_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        LOG("property change of unmanaged window: %" PRId32 "\n",
                event->window);
        return;
    }
    if (event->atom == XCB_ATOM_WM_NAME) {
        update_window_name(window);
    } else if (event->atom == XCB_ATOM_WM_SIZE_HINTS) {
        update_window_size_hints(window);
    }
    set_window_state(window, predict_window_state(window), 0);
}

/* Unmap notifications are sent when a window decided wanted to not be seen
 * anymore.
 */
void handle_unmap_notify(xcb_unmap_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
        set_window_state(window, WINDOW_STATE_HIDDEN, 1);
    }
}

/* Destroy notifications are sent when a window leaves the X server,
 * good bye to that window!
 */
static void handle_destroy_notify(xcb_destroy_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
        destroy_window(window);
    }
}

/* Key press events are sent when a GRABBED key is triggered, keys were
 * grabbed at the start in setup_keys() using xcb_grab_key().
 */
void handle_key_press(xcb_key_press_event_t *event)
{
    int action;

    action = get_action_bind(event);
    if (action != -1) {
        do_action(action);
    }
}

/* Little story: When xterm is opened, it waits a few seconds to appear on
 * screen. However, when the window manager answers to xterm's configure
 * request, it opens instantly which induces the TODO: what is xterm really
 * waiting for?
 *
 * Configure requests are not important at all, they should be disregarded
 * for all windows OR properly managed which induces a TODO.
 */
void handle_configure_request(xcb_configure_request_event_t *event)
{
    g_values[0] = event->x;
    g_values[1] = event->y;
    g_values[2] = event->width;
    g_values[3] = event->height;
    g_values[4] = event->border_width;
    xcb_configure_window(g_dpy, event->window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);
}

/* Handle the given xcb event.
 *
 * Descriptions for each event are above each handler.
 *
 * TODO: What is the best way to handle focus in/out events?
 */
void handle_event(xcb_generic_event_t *event)
{
#ifdef DEBUG
    log_event(event);
#endif

    switch ((event->response_type & ~0x80)) {
    case XCB_MAP_REQUEST:
        handle_map_request((xcb_map_request_event_t*) event);
        break;
    case XCB_BUTTON_PRESS:
        handle_button_press((xcb_button_press_event_t*) event);
        break;
    case XCB_MOTION_NOTIFY:
        handle_motion_notify((xcb_motion_notify_event_t*) event);
        break;
    case XCB_BUTTON_RELEASE:
        handle_button_release((xcb_button_release_event_t*) event);
        break;
    case XCB_PROPERTY_NOTIFY:
        handle_property_notify((xcb_property_notify_event_t*) event);
        break;
    case XCB_UNMAP_NOTIFY:
        handle_unmap_notify((xcb_unmap_notify_event_t*) event);
        break;
    case XCB_DESTROY_NOTIFY:
        handle_destroy_notify((xcb_destroy_notify_event_t*) event);
        break;
    /*case XCB_FOCUS_IN:
        window = get_window_of_xcb_window(
                ((xcb_focus_in_event_t*) event)->event);
        if (window != NULL) {
            LOG("setting focus of window %" PRIu32 " to 1\n", window->number);
            window->focused = 1;
        }
        break;

    case XCB_FOCUS_OUT:
        window = get_window_of_xcb_window(
                ((xcb_focus_out_event_t*) event)->event);
        if (window != NULL) {
            LOG("setting focus of window %" PRIu32 " to 0\n", window->number);
            window->focused = 0;
        }
        break;*/
    case XCB_CONFIGURE_REQUEST:
        handle_configure_request((xcb_configure_request_event_t*) event);
        break;
    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        break;
    }
}
