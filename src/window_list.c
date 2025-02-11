#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "render_font.h"
#include "screen.h"
#include "utility.h"
#include "window.h"

static inline bool is_valid_for_display(Window *window)
{
    return window->state.was_ever_mapped && does_window_accept_focus(window);
}

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
    FcChar8                 buffer[256];
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

    if (window_count == 0) {
        return ERROR;
    }

    primary_frame = get_primary_monitor()->frame;
    general_values[0] = primary_frame->x + primary_frame->width - max_width -
        WINDOW_PADDING;
    general_values[1] = primary_frame->y;
    general_values[2] = max_width + WINDOW_PADDING / 2;
    general_values[3] = window_count *
        (measure.ascent - measure.descent + WINDOW_PADDING);
    general_values[4] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, screen->window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            screen->window_list_window, XCB_CURRENT_TIME);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + WINDOW_PADDING / 2;
    rectangle.height = measure.ascent - measure.descent + WINDOW_PADDING;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        background_color.alpha = 0xff00;
        if (window == selected) {
            pen = screen->stock_objects[STOCK_WHITE_PEN];
            background_color.red = 0x0000;
            background_color.green = 0x0000;
            background_color.blue = 0x0000;
        } else {
            pen = screen->stock_objects[STOCK_BLACK_PEN];
            background_color.red = 0xff00;
            background_color.green = 0xff00;
            background_color.blue = 0xff00;
        }

        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->properties.name);
        draw_text(screen->window_list_window, buffer,
                strlen((char*) buffer), background_color, &rectangle,
                pen, WINDOW_PADDING / 2,
                rectangle.y + measure.ascent + WINDOW_PADDING / 2);
        rectangle.y += rectangle.height;
    }
    return OK;
}

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

static inline int handle_key_press(Window **pointer_selected,
        xcb_key_press_event_t *event)
{
    Window *selected;

    selected = *pointer_selected;

    switch (get_keysym(event->detail)) {
    case XK_q:
    case XK_Escape:
        selected = NULL;
        break;

    case XK_c:
    case XK_Return:
        return 1;

    case XK_Home:
        selected = get_valid_window_after(NULL, first_window);
        break;

    case XK_End:
        selected = get_valid_window_before(selected, first_window, NULL);
        break;

    case XK_h:
    case XK_k:
    case XK_Left:
    case XK_Up:
        selected = get_valid_window_before(selected, first_window, selected);
        break;

    case XK_l:
    case XK_j:
    case XK_Right:
    case XK_Down:
        selected = get_valid_window_after(selected, selected->next);
        break;
    }

    *pointer_selected = selected;
    return 0;
}

/* Handle all incoming xcb events for the window list. */
static inline Window *handle_window_list_events(Window *selected)
{
    xcb_generic_event_t *event;
    Window *window;

    while (xcb_connection_has_error(connection) == 0) {
        if (selected == NULL) {
            return NULL;
        }
        if (render_window_list(selected) != 0) {
            return NULL;
        }

        /* make sure the window list keeps focus */
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            screen->window_list_window, XCB_CURRENT_TIME);

        xcb_flush(connection);

        event = xcb_wait_for_event(connection);
        if (event == NULL) {
            continue;
        }

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
        set_notification((FcChar8*) "No managed windows",
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        return NULL;
    }

    xcb_map_window(connection, screen->window_list_window);

    window = handle_window_list_events(window);

    xcb_unmap_window(connection, screen->window_list_window);

    if (focus_window != NULL) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            focus_window->xcb_window, XCB_CURRENT_TIME);
    }

    return window;
}
