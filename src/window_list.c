#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "stash_frame.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"

/* user window list window */
struct window_list window_list;

/* Create the window list. */
int initialize_window_list(void)
{
    xcb_generic_error_t *error;
    const char *window_list_name = "[fensterchef] window list";

    window_list.client.id = xcb_generate_id(connection);
    window_list.client.x = -1;
    window_list.client.y = -1;
    window_list.client.width = 1;
    window_list.client.height = 1;
    window_list.client.background_color = 0xffffff;
    window_list.client.border_color = 0x000000;
    window_list.client.border_width = 1;

    general_values[0] = window_list.client.background_color;
    general_values[1] = window_list.client.border_color;
    /* indicate to not manage the window */
    general_values[2] = true;
    /* get key press events, focus change events and expose events */
    general_values[3] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    error = xcb_request_check(connection, xcb_create_window_checked(connection,
                XCB_COPY_FROM_PARENT, window_list.client.id,
                screen->root, window_list.client.x, window_list.client.y,
                window_list.client.width, window_list.client.height,
                window_list.client.border_width, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                    XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
                    general_values));
    if (error != NULL) {
        LOG_ERROR("could not create window list window: %E\n", error);
        free(error);
        return ERROR;
    }
    xcb_icccm_set_wm_name(connection, window_list.client.id,
            XCB_ATOM_STRING, 8, strlen(window_list_name), window_list_name);
    return OK;
}

/* Check if @window should appear in the window list. */
static bool is_window_in_window_list(Window *window)
{
    return is_window_focusable(window);
}

/* Get character indicating the window state. */
static inline char get_indicator_character(Window *window)
{
    return window == Window_focus ? '*' :
            !window->state.is_visible ? '-' :
            window->state.mode == WINDOW_MODE_FLOATING ? '=' :
            window->state.mode == WINDOW_MODE_FULLSCREEN ? 'F' : '+';
}

/* Get a string representation of a window. */
static inline void get_window_string(Window *window, utf8_t *buffer,
        size_t buffer_size)
{
    snprintf((char*) buffer, buffer_size, "%" PRIu32 "%c%s",
            window->number, get_indicator_character(window), window->name);
}

/* Get the window currently selected in the window list. */
static Window *get_selected_window(void)
{
    uint32_t index;

    index = window_list.selected;
    for (Window *window = Window_first; window != NULL; window = window->next) {
        if (!is_window_in_window_list(window)) {
            continue;
        }
        if (index == 0) {
            return window;
        }
        index--;
    }
    return NULL;
}

/* Render the window list. */
static void render_window_list(void)
{
    utf8_t                  buffer[256];
    uint32_t                window_count;
    struct text_measure     measure;
    uint32_t                height_per_item;
    uint32_t                max_width;
    Monitor                 *monitor;
    uint32_t                index;
    uint32_t                maximum_item;
    xcb_rectangle_t         rectangle;

    /* measure the maximum needed width and count the windows */
    window_count = 0;
    max_width = 0;
    for (Window *window = Window_first; window != NULL; window = window->next) {
        if (!is_window_in_window_list(window)) {
            continue;
        }

        window_count++;
        get_window_string(window, buffer, sizeof(buffer));
        measure_text(buffer, strlen((char*) buffer), &measure);
        max_width = MAX(max_width, measure.total_width);
    }

    /* unmap the window list if there are no more windows */
    if (window_count == 0) {
        unmap_client(&window_list.client);
        return;
    }

    window_list.selected = MIN(window_list.selected, window_count - 1);

    height_per_item = measure.total_height + configuration.notification.padding;

    if (Window_focus == NULL || Frame_focus->window == Window_focus) {
        monitor = get_monitor_containing_frame(Frame_focus);
    } else {
        monitor = get_monitor_from_rectangle_or_primary(
                Window_focus->x, Window_focus->y, Window_focus->width,
                Window_focus->height);
    }

    /* the number of items that can fit on screen */
    maximum_item = (monitor->height - configuration.notification.border_size) /
        height_per_item + 1;
    maximum_item = MIN(maximum_item, window_count);

    /* adjust the scrolling so the selected item is visible */
    if (window_list.selected < window_list.vertical_scrolling) {
        window_list.vertical_scrolling = window_list.selected;
    }
    if (window_list.selected >= window_list.vertical_scrolling + maximum_item) {
        window_list.vertical_scrolling = window_list.selected - maximum_item + 1;
    }

    /* set the list position and size so it is in the top right of the monitor
     * containing the focus frame
     */
    configure_client(&window_list.client,
            monitor->x + monitor->width - max_width -
                configuration.notification.padding / 2 -
                configuration.notification.border_size * 2,
            monitor->y,
            max_width + configuration.notification.padding / 2,
            maximum_item * height_per_item,
            window_list.client.border_width);

    /* render the items showing the window names */
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + configuration.notification.padding / 2;
    rectangle.height = height_per_item;
    index = 0;
    for (Window *window = Window_first; window != NULL; window = window->next) {
        uint32_t background_color, foreground_color;

        if (!is_window_in_window_list(window)) {
            continue;
        }

        index++;
        /* if the item is above the scrolling, ignore it */
        if (index <= window_list.vertical_scrolling) {
            continue;
        }
        /* if the item is below the visible region, stop */
        if (index > window_list.vertical_scrolling + maximum_item) {
            break;
        }

        /* use normal or inverted colors */
        if (index - 1 != window_list.selected) {
            foreground_color = configuration.notification.foreground;
            background_color = configuration.notification.background;
        } else {
            foreground_color = configuration.notification.background;
            background_color = configuration.notification.foreground;
        }

        /* draw the text centered within the item */
        get_window_string(window, buffer, sizeof(buffer));
        draw_text(window_list.client.id, buffer, strlen((char*) buffer),
                background_color, &rectangle,
                foreground_color, configuration.notification.padding / 2,
                rectangle.y + measure.ascent +
                    configuration.notification.padding / 2);

        rectangle.y += rectangle.height;
    }
}

/* Show @window and focus it.
 *
 * @shift controls how the window appears. Either it is put directly into the
 *        current frame or simply shown which allows auto splitting to occur.
 */
static inline void focus_and_let_window_appear(Window *window, bool shift)
{
    /* if shift is down, show the window in the current frame */
    if (shift && !window->state.is_visible) {
        stash_frame(Frame_focus);
        set_window_mode(window, WINDOW_MODE_TILING);
    } else {
        /* put floating windows on the top */
        update_window_layer(window);
    }
    show_window(window);
    set_focus_window_with_frame(window);
}

/* Handle a key press for the window list window. */
static void handle_key_press(xcb_key_press_event_t *event)
{
    Window *selected;

    if (event->event != window_list.client.id) {
        return;
    }

    switch (get_keysym(event->detail)) {
    /* cancel selection */
    case XK_q:
    case XK_n:
    case XK_Escape:
        unmap_client(&window_list.client);
        break;

    /* confirm selection */
    case XK_y:
    case XK_Return:
        selected = get_selected_window();
        if (selected != NULL && selected != Window_focus) {
            focus_and_let_window_appear(selected,
                    !!(event->state & XCB_MOD_MASK_SHIFT));
            window_list.should_revert_focus = false;
        }
        unmap_client(&window_list.client);
        break;

    /* go to the first item */
    case XK_Home:
        window_list.selected = 0;
        break;

    /* go to the last item */
    case XK_End:
        window_list.selected = INT32_MAX;
        break;

    /* go to the previous item */
    case XK_h:
    case XK_k:
    case XK_Left:
    case XK_Up:
        if (window_list.selected > 0) {
            window_list.selected--;
        }
        break;

    /* go to the next item */
    case XK_l:
    case XK_j:
    case XK_Right:
    case XK_Down:
        window_list.selected++;
        break;
    }
}

/* Handle a FocusOut event. */
static void handle_focus_out(xcb_focus_out_event_t *event)
{
    /* if the this event is for the window list and it is mapped (we also get
     * this event after the window was unmapped)
     */
    if (event->event != window_list.client.id ||
            !window_list.client.is_mapped) {
        return;
    }

    /* if someone grabbed the focus, we expect it to return soon */
    if (event->mode == XCB_NOTIFY_MODE_GRAB) {
        return;
    }

    /* refocus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
            window_list.client.id, XCB_CURRENT_TIME);
}

/* Handle an UnmapNotify event. */
static void handle_unmap_notify(xcb_unmap_notify_event_t *event)
{
    if (event->window != window_list.client.id) {
        return;
    }

    /* give focus back to the last window */
    if (window_list.should_revert_focus) {
        set_input_focus(Window_focus);
    }
}

/* Handle an incoming event for the window list. */
void handle_window_list_event(xcb_generic_event_t *event)
{
    /* remove the most significant bit, this gets the actual event type */
    switch ((event->response_type & ~0x80)) {
    /* an error */
    case 0:
        return;

    /* a key was pressed */
    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        break;

    /* the window list lost focus */
    case XCB_FOCUS_OUT:
        handle_focus_out((xcb_focus_out_event_t*) event);
        break;

    /* the window list was hidden */
    case XCB_UNMAP_NOTIFY:
        handle_unmap_notify((xcb_unmap_notify_event_t*) event);
        break;
    }

    /* re-rendering after every event allows to react to dynamic changes */
    if (window_list.client.is_mapped) {
        render_window_list();
    }
}

/* Show the window list on screen. */
int show_window_list(void)
{
    uint32_t index = 0;
    Window *selected;

    /* if the window list is already shown, do nothing */
    if (window_list.client.is_mapped) {
        return ERROR;
    }

    /* get the initially selected window */
    if (Window_focus != NULL) {
        selected = Window_focus;
    } else {
        for (selected = Window_first; selected != NULL;
                selected = selected->next) {
            if (is_window_in_window_list(selected)) {
                break;
            }
            index++;
        }
    }

    if (selected == NULL) {
        set_notification((utf8_t*) "No managed windows",
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        return ERROR;
    }

    window_list.selected = index;
    window_list.should_revert_focus = true;

    /* show the window list window on screen */
    map_client(&window_list.client);

    /* raise the window */
    general_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, window_list.client.id,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* focus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
            window_list.client.id, XCB_CURRENT_TIME);
    return OK;
}
