#include <inttypes.h>

#include <xcb/randr.h>

#include "configuration.h"
#include "fensterchef.h"
#include "frame.h"
#include "keybind.h"
#include "keymap.h"
#include "log.h"
#include "root_properties.h"
#include "screen.h"
#include "util.h"
#include "window.h"

/* This file handles all kinds of xcb events.
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

/* this is used for moving a popup window */
static struct {
    /* the initial position of the moved window (for ESCAPE cancellation) */
    Position start;
    /* previous mouse position, needed to compute the mouse motion */
    Position old_mouse;
    /* the window that is being moved */
    xcb_window_t xcb_window;
} selected_window;

/* Create notifications are sent when a client creates a window on our
 * connection.
 */
static void handle_create_notify(xcb_create_notify_event_t *event)
{
    Window *window;

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
        /* can this case ever happen? */
        return;
    }
    show_window(window);
    if (!does_window_have_type(window, ewmh._NET_WM_WINDOW_TYPE_DESKTOP) &&
            !does_window_have_type(window, ewmh._NET_WM_WINDOW_TYPE_DOCK)) {
        set_focus_window(window);
    }
}

/* Button press events are sent when the mouse is pressed together with
 * the special modifier key. This is used to move popup windows.
 */
static void handle_button_press(xcb_button_press_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->child);
    if (window == NULL || window->state.mode != WINDOW_MODE_POPUP) {
        return;
    }

    selected_window.start.x = event->root_y;
    selected_window.start.y = event->root_y;

    selected_window.old_mouse.x = event->root_x;
    selected_window.old_mouse.y = event->root_y;

    selected_window.xcb_window = event->child;

    xcb_grab_pointer(connection, 0, event->root,
            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, event->root,
            XCB_NONE, XCB_CURRENT_TIME);
}

/* Motion notifications (mouse move events) are only sent when we grabbed them.
 * This only happens when a popup window is being moved.
 */
static void handle_motion_notify(xcb_motion_notify_event_t *event)
{
    xcb_get_geometry_reply_t *geometry;

    geometry = xcb_get_geometry_reply(connection,
            xcb_get_geometry(connection, selected_window.xcb_window), NULL);
    if (geometry == NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        selected_window.xcb_window = XCB_NONE;
        return;
    }
    general_values[0] = geometry->x + (event->root_x - selected_window.old_mouse.x);
    general_values[1] = geometry->y + (event->root_y - selected_window.old_mouse.y);
    selected_window.old_mouse.x = event->root_x;
    selected_window.old_mouse.y = event->root_y;
    xcb_configure_window(connection, selected_window.xcb_window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, general_values);
}

/* Button releases are only sent when we grabbed them. This only happens
 * when a popup window is being moved.
 */
static void handle_button_release(xcb_button_release_event_t *event)
{
    (void) event;
    xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
    selected_window.xcb_window = XCB_NONE;
}

/* Property notifications are sent when a window atom changes, this can
 * be many atoms, but the main ones handled are WM_NAME, WM_SIZE_HINTS,
 * WM_HINTS.
 */
static void handle_property_notify(xcb_property_notify_event_t *event)
{
    Window *window;

    window = get_window_of_xcb_window(event->window);
    if (window == NULL) {
        if (event->window != screen->xcb_screen->root) {
            LOG("property change of unmanaged window: %" PRId32 "\n", event->window);
        }
        return;
    }

    if (!update_window_property_by_atom(window, event->atom)) {
        /* nothing we know of changed */
        return;
    }

    set_window_mode(window, predict_window_mode(window), 0);

    if (window->state.is_visible && !is_strut_empty(&window->properties.struts)) {
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

    if (event->window == selected_window.xcb_window) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        selected_window.xcb_window = XCB_NONE;
    }

    window = get_window_of_xcb_window(event->window);
    if (window != NULL) {
        hide_window(window);
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

/* Key press events are sent when a GRABBED key is triggered, keys were
 * grabbed at the start in init_keybinds() using xcb_grab_key().
 */
void handle_key_press(xcb_key_press_event_t *event)
{
    action_t action;

    action = find_configured_action(event->state, get_keysym(event->detail));
    if (action != ACTION_NULL) {
        LOG("performing action: %u\n", action);
        do_action(action);
    } else {
        LOG("trash key: %d\n", event->detail);
    }
}

/* Configure requests are received when a window wants to choose its own
 * position and size.
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

/* Focus in events are sent when a window received focus. */
void handle_focus_in(xcb_focus_in_event_t *event)
{
    Window *window;

    /* TODO: describe this */
    if (event->mode != XCB_NOTIFY_MODE_NORMAL ||
            event->detail == XCB_NOTIFY_DETAIL_POINTER) {
        return;
    }

    window = get_window_of_xcb_window(event->event);
    if (window == NULL || window == focus_window) {
        return;
    }
    set_focus_window_with_frame(window);
}

/* Handle the given xcb event.
 *
 * Descriptions for each event are above each handler.
 */
void handle_event(xcb_generic_event_t *event)
{
    uint8_t type;

    type = (event->response_type & ~0x80);

    if (type >= randr_event_base) {
        /* TODO: there are more randr events? what do they mean? */
        handle_screen_change((xcb_randr_screen_change_notify_event_t*) event);
        return;
    }

    log_event(event);

    switch (type) {
    /* a window was created */
    case XCB_CREATE_NOTIFY:
        handle_create_notify((xcb_create_notify_event_t*) event);
        break;

    /* a window wants to appear on the screen */
    case XCB_MAP_REQUEST:
        handle_map_request((xcb_map_request_event_t*) event);
        break;

    /* a window was focused */
    case XCB_FOCUS_IN:
        handle_focus_in((xcb_focus_in_event_t*) event);
        break;

    /* a mouse button was pressed */
    case XCB_BUTTON_PRESS:
        handle_button_press((xcb_button_press_event_t*) event);
        break;

    /* the mouse was moved */
    case XCB_MOTION_NOTIFY:
        handle_motion_notify((xcb_motion_notify_event_t*) event);
        break;

    /* a mouse button was released */
    case XCB_BUTTON_RELEASE:
        handle_button_release((xcb_button_release_event_t*) event);
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
        break;

    /* keyboard mapping changed */
    case XCB_MAPPING_NOTIFY:
        handle_mapping_notify((xcb_mapping_notify_event_t*) event);
        break;
    }
}
