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

/* Check if @window should appear in the window list. */
static inline bool is_valid_for_display(Window *window)
{
    return window->state.was_ever_mapped && does_window_accept_focus(window);
}

/* Get character indicating the window state. */
static inline char get_indicator_character(Window *window)
{
    return !window->state.is_visible ? '-' :
            window->state.mode == WINDOW_MODE_POPUP ?
                (window == focus_window ? '#' : '=') :
            window->state.mode == WINDOW_MODE_FULLSCREEN ?
                (window == focus_window ? '@' : 'F') :
            window == focus_window ? '*' : '+';
}

/* Render the window list.
 *
 * TODO: add scrolling
 */
static int render_window_list(Window *selected)
{
    utf8_t                  buffer[256];
    uint32_t                window_count;
    struct text_measure     measure;
    uint32_t                max_width;
    struct frame            *primary_frame;
    xcb_rectangle_t         rectangle;
    xcb_render_color_t      background_color;
    xcb_render_picture_t    pen;

    /* measure the maximum needed width */
    window_count = 0;
    measure.ascent = 12;
    measure.descent = -4;
    max_width = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        window_count++;
        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->properties.name);
        measure_text(buffer, strlen((char*) buffer), &measure);
        max_width = MAX(max_width, measure.total_width);
    }

    /* return ERROR to indicate the window list should close */
    if (window_count == 0) {
        return ERROR;
    }

    /* set the list position and size so it is in the top right of the primary
     * monitor
     */
    primary_frame = get_primary_monitor()->frame;
    general_values[0] = primary_frame->x + primary_frame->width - max_width -
        configuration.notification.padding / 2 -
        configuration.notification.border_size * 2;
    general_values[1] = primary_frame->y;
    general_values[2] = max_width + configuration.notification.padding / 2;
    general_values[3] = window_count *
        (measure.ascent - measure.descent + configuration.notification.padding);
    general_values[4] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* focus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            window_list_window, XCB_CURRENT_TIME);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + configuration.notification.padding / 2;
    rectangle.height = measure.ascent - measure.descent +
        configuration.notification.padding;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        if (window != selected) {
            pen = stock_objects[STOCK_BLACK_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.background);
        } else {
            pen = stock_objects[STOCK_WHITE_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.foreground);
        }

        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->properties.name);
        draw_text(window_list_window, buffer,
                strlen((char*) buffer), background_color, &rectangle,
                pen, configuration.notification.padding / 2,
                rectangle.y + measure.ascent +
                    configuration.notification.padding / 2);
        rectangle.y += rectangle.height;
    }
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
static inline bool handle_key_press(Window **pointer_selected,
        xcb_key_press_event_t *event)
{
    Window *selected;

    selected = *pointer_selected;

    switch (get_keysym(event->detail)) {
    /* cancel selection */
    case XK_q:
    case XK_Escape:
        selected = NULL;
        break;

    /* confirm selection */
    case XK_c:
    case XK_Return:
        return true;

    /* go to the first item */
    case XK_Home:
        selected = get_valid_window_after(NULL, first_window);
        break;

    /* go to the last item */
    case XK_End:
        selected = get_valid_window_before(selected, first_window, NULL);
        break;

    /* go to the previous item */
    case XK_h:
    case XK_k:
    case XK_Left:
    case XK_Up:
        selected = get_valid_window_before(selected, first_window, selected);
        break;

    /* go to the next item */
    case XK_l:
    case XK_j:
    case XK_Right:
    case XK_Down:
        selected = get_valid_window_after(selected, selected->next);
        break;
    }

    *pointer_selected = selected;
    return false;
}

/* Handle all incoming xcb events for the window list. */
static inline Window *handle_window_list_events(Window *selected)
{
    xcb_generic_event_t *event;
    Window *window;

    while (true) {
        /* all windows left or the user cancelled selecting */
        if (selected == NULL) {
            return NULL;
        }

        if (render_window_list(selected) != OK) {
            return NULL;
        }

        xcb_flush(connection);

        event = xcb_wait_for_event(connection);
        if (event == NULL) {
            break;
        }

        LOG("");
        log_event(event);

        /* remove the most significant bit, this gets the actual event type */
        switch ((event->response_type & ~0x80)) {
        case XCB_KEY_PRESS:
            if (handle_key_press(&selected, (xcb_key_press_event_t*) event)) {
                free(event);
                return selected;
            }
            break;

        /* make sure to selected another window if the selected one is destroyed */
        case XCB_DESTROY_NOTIFY:
            window = get_window_of_xcb_window(
                    ((xcb_unmap_notify_event_t*) event)->window);
            if (window == selected) {
                selected = get_valid_window_before(
                        get_valid_window_after(NULL, selected->next),
                        first_window, selected);
            }
        /* fall through */
        default:
            handle_event(event);
        }
        free(event);
    }
    return NULL;
}

/* Shows a windows list where the user can select a window from. */
Window *select_window_from_list(void)
{
    Window *window;

    /* get the initially selected window */
    if (focus_window != NULL) {
        window = focus_window;
    } else {
        for (window = first_window; window != NULL; window = window->next) {
            if (is_valid_for_display(window)) {
                break;
            }
        }
    }

    if (window == NULL) {
        set_notification((uint8_t*) "No managed windows",
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        return NULL;
    }

    /* show the window on screen */
    xcb_map_window(connection, window_list_window);

    window = handle_window_list_events(window);

    /* hide the window from the screen */
    xcb_unmap_window(connection, window_list_window);

    /* refocus the old window */
    if (focus_window != NULL) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            focus_window->properties.window, XCB_CURRENT_TIME);
    }

    return window;
}
