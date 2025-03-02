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
#include "utility.h"
#include "window.h"

/* the currently selected window */
Window *selected;
/* the currently scrolled amount */
uint32_t vertical_scrolling;

/* Check if @window should appear in the window list. */
static inline bool is_valid_for_display(Window *window)
{
    return does_window_accept_focus(window);
}

/* Get character indicating the window state. */
static inline char get_indicator_character(Window *window)
{
    return !window->state.is_visible ? '-' :
            window->state.mode == WINDOW_MODE_FLOATING ?
                (window == focus_window ? '#' : '=') :
            window->state.mode == WINDOW_MODE_FULLSCREEN ?
                (window == focus_window ? '@' : 'F') :
            window == focus_window ? '*' : '+';
}

/* Render the window list. */
static int render_window_list(void)
{
    utf8_t                  buffer[256];
    uint32_t                window_count;
    struct text_measure     measure;
    uint32_t                height_per_item;
    uint32_t                max_width;
    Frame                   *root_frame;
    uint32_t                index = 0;
    uint32_t                maximum_item;
    xcb_rectangle_t         rectangle;
    xcb_render_color_t      background_color;
    xcb_render_picture_t    pen;

    /* measure the maximum needed width and get the index of the currently
     * selected window
     */
    window_count = 0;
    measure.ascent = 12;
    measure.descent = -4;
    max_width = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        if (selected == window) {
            index = window_count;
        }

        window_count++;
        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->name);
        measure_text(buffer, strlen((char*) buffer), &measure);
        max_width = MAX(max_width, measure.total_width);
    }

    /* return ERROR to indicate the window list should close */
    if (window_count == 0) {
        return ERROR;
    }

    height_per_item = measure.ascent - measure.descent +
        configuration.notification.padding;

    root_frame = get_root_frame(focus_frame);

    /* the number of items that can fit on screen */
    maximum_item = (root_frame->height -
            configuration.notification.border_size) / height_per_item;
    maximum_item = MIN(maximum_item, window_count);

    /* adjust the scrolling so the selected item is visible */
    if (index < vertical_scrolling) {
        vertical_scrolling = index;
    }
    if (index >= vertical_scrolling + maximum_item) {
        vertical_scrolling = index - maximum_item + 1;
    }

    /* set the list position and size so it is in the top right of the monitor
     * containing the focus frame
     */
    configure_client(&window_list,
            root_frame->x + root_frame->width - max_width -
                configuration.notification.padding / 2 -
                configuration.notification.border_size * 2,
            root_frame->y,
            max_width + configuration.notification.padding / 2,
            maximum_item * height_per_item,
            window_list.border_width);

    /* raise the window */
    general_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, window_list.id,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* focus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE, window_list.id,
            XCB_CURRENT_TIME);

    /* render the items showing the window names */
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + configuration.notification.padding / 2;
    rectangle.height = height_per_item;
    index = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        index++;
        /* if the item is above the scrolling, ignore it */
        if (index <= vertical_scrolling) {
            continue;
        }
        /* if the item is below the visible region, stop */
        if (index > vertical_scrolling + maximum_item) {
            break;
        }

        /* use normal or inverted colors */
        if (window != selected) {
            pen = stock_objects[STOCK_BLACK_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.background);
        } else {
            pen = stock_objects[STOCK_WHITE_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.foreground);
        }

        /* draw the text centered within the item */
        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->name);
        draw_text(window_list.id, buffer,
                strlen((char*) buffer), background_color, &rectangle,
                pen, configuration.notification.padding / 2,
                rectangle.y + measure.ascent +
                    configuration.notification.padding / 2);

        rectangle.y += rectangle.height;
    }

    /* flush after rendering so all becomes visible immediately */
    xcb_flush(connection);
    return OK;
}

/* Get the window before @start in the window list. @last_valid is the fallback
 * value and @before is the end point.
 */
static inline Window *get_valid_window_before(Window *last_valid, Window *start,
        Window *before)
{
    while (start != before) {
        if (is_valid_for_display(start)) {
            last_valid = start;
        }
        start = start->next;
    }
    return last_valid;
}

/* Get the window after @start. @last_valid is the fallback value. This does not
 * have a third parameter because it would just be NULL anyway for all calls.
 */
static inline Window *get_valid_window_after(Window *last_valid, Window *start)
{
    while (start != NULL) {
        if (is_valid_for_display(start)) {
            last_valid = start;
            break;
        }
        start = start->next;
    }
    return last_valid;
}

/* Handle a key press for the window list window.
 *
 * @return if the selection was confirmed.
 */
static inline bool handle_key_press(xcb_key_press_event_t *event)
{
    switch (get_keysym(event->detail)) {
    /* cancel selection */
    case XK_q:
    case XK_n:
    case XK_Escape:
        selected = NULL;
        break;

    /* confirm selection */
    case XK_y:
    case XK_Return:
        return true;

    /* go to the first item */
    case XK_Home:
        selected = get_valid_window_after(NULL, first_window);
        break;

    /* go to the last item */
    case XK_End:
        selected = get_valid_window_before(selected,
                first_window, NULL);
        break;

    /* go to the previous item */
    case XK_h:
    case XK_k:
    case XK_Left:
    case XK_Up:
        selected = get_valid_window_before(selected,
                first_window, selected);
        break;

    /* go to the next item */
    case XK_l:
    case XK_j:
    case XK_Right:
    case XK_Down:
        selected = get_valid_window_after(selected,
                selected->next);
        break;
    }
    return false;
}

/* Handle all incoming xcb events for the window list. */
static int window_list_callback(xcb_generic_event_t *event)
{
    Window *window;

    /* remove the most significant bit, this gets the actual event type */
    switch ((event->response_type & ~0x80)) {
    case XCB_KEY_PRESS:
        if (((xcb_key_press_event_t*) event)->event == window_list.id) {
            if (handle_key_press((xcb_key_press_event_t*) event)) {
                free(event);
                return ERROR;
            }
        }
        break;

    /* select another window if the currently selected one is destroyed */
    case XCB_DESTROY_NOTIFY:
        window = get_window_of_xcb_window(
                ((xcb_unmap_notify_event_t*) event)->window);
        if (window == selected) {
            selected = get_valid_window_before(
                    get_valid_window_after(NULL, selected->next),
                    first_window, selected);
        }
        break;
    }
    return OK;
}

/* Shows a windows list where the user can select a window from. */
static Window *select_window_from_list(void)
{
    Window *old_focus_window;

    /* do not show the window list if it is already there */
    if (window_list.is_mapped) {
        return NULL;
    }

    old_focus_window = focus_window;

    /* get the initially selected window */
    if (focus_window != NULL) {
        selected = focus_window;
    } else {
        for (selected = first_window; selected != NULL;
                selected = selected->next) {
            if (is_valid_for_display(selected)) {
                break;
            }
        }
    }

    if (selected == NULL) {
        set_notification((uint8_t*) "No managed windows",
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        return NULL;
    }

    /* show the window list window on screen */
    map_client(&window_list);

    /* if this is called from an event, we need to unfreeze the keyboard and
     * pointer first to receive events
     */
    xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
    xcb_allow_events(connection, XCB_ALLOW_ASYNC_KEYBOARD, XCB_CURRENT_TIME);
    /* flush before taking control */
    xcb_flush(connection);
    /* take control of the event loop */
    while (render_window_list() != ERROR) {
        if (next_cycle(window_list_callback) != OK) {
            break;
        }
    }

    /* hide the window list window from the screen */
    unmap_client(&window_list);

    /* the window list stole the focus, give it back */
    if (old_focus_window == focus_window) {
        if (focus_window == NULL) {
            xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                    wm_check_window, XCB_CURRENT_TIME);
        } else {
            xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                    focus_window->client.id, XCB_CURRENT_TIME);
        }
    }

    return selected;
}

/* Shows the window list and shows/focuses the selected window. */
void show_list_and_show_selected_window(void)
{
    Window *window;

    window = select_window_from_list();
    if (window == NULL || window == focus_window) {
        return;
    }

    /* put floating windows on the top */
    update_window_layer(window);

    show_window(window);
    set_focus_window_with_frame(window);
}
