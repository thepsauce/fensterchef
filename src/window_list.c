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
#include "util.h"
#include "window.h"

static char get_indicator_character(Window *window)
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
    struct text_measure     tm;
    uint32_t                max_width;
    struct frame            *primary_frame;
    xcb_rectangle_t         rectangle;
    xcb_render_color_t      background_color;
    xcb_render_picture_t    pen;

    /* measure the maximum needed width */
    window_count = 0;
    tm.ascent = 12;
    tm.descent = -4;
    max_width = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!window->state.was_ever_mapped) {
            continue;
        }

        window_count++;
        snprintf((char*) buffer, sizeof(buffer), "%" PRIu32 "%c%s",
                window->number, get_indicator_character(window),
                window->properties.name);
        measure_text(buffer, strlen((char*) buffer), &tm);
        max_width = MAX(max_width, tm.total_width);
    }

    if (window_count == 0) {
        return 1;
    }

    primary_frame = get_primary_monitor()->frame;
    general_values[0] = primary_frame->x + primary_frame->width - max_width -
        WINDOW_PADDING;
    general_values[1] = primary_frame->y;
    general_values[2] = max_width + WINDOW_PADDING / 2;
    general_values[3] = window_count *
        (tm.ascent - tm.descent + WINDOW_PADDING);
    general_values[4] = 1;
    general_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, screen->window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH |
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
            screen->window_list_window, XCB_CURRENT_TIME);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + WINDOW_PADDING / 2;
    rectangle.height = tm.ascent - tm.descent + WINDOW_PADDING;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!window->state.was_ever_mapped) {
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
                rectangle.y + tm.ascent + WINDOW_PADDING / 2);
        rectangle.y += rectangle.height;
    }
    return 0;
}

/* Handle all incoming xcb events for the window list. */
static inline Window *handle_window_list_events(Window *selected,
        Window **p_old_focus)
{
    xcb_generic_event_t *event;
    xcb_key_press_event_t *key_press;
    xcb_keysym_t keysym;
    Window *window;

    while (xcb_connection_has_error(connection) == 0) {
        if (render_window_list(selected) != 0) {
            return NULL;
        }

        xcb_flush(connection);

        event = xcb_wait_for_event(connection);
        if (event == NULL) {
            continue;
        }

        switch ((event->response_type & ~0x80)) {
        case XCB_KEY_PRESS:
            key_press = (xcb_key_press_event_t*) event;
            keysym = get_keysym(key_press->detail);
            switch (keysym) {
            case XK_q:
            case XK_Escape:
                free(event);
                return NULL;

            case XK_c:
            case XK_Return:
                free(event);
                return selected;

            case XK_Home:
                selected = first_window;
                break;

            case XK_End:
                selected = first_window;
                while (selected->next != NULL) {
                    selected = selected->next;
                }
                break;

            case XK_k:
            case XK_Up:
                selected = get_previous_window(selected);
                break;

            case XK_j:
            case XK_Down:
                selected = selected->next;
                if (selected == NULL) {
                    selected = first_window;
                }
                break;
            }
            break;

        case XCB_DESTROY_NOTIFY:
            window = get_window_of_xcb_window(
                    ((xcb_unmap_notify_event_t*) event)->window);
            if (window != NULL) {
                if (window == *p_old_focus) {
                    *p_old_focus = NULL;
                }

                if (window == selected) {
                    selected = selected->next;
                    if (selected == NULL) {
                        selected = first_window;
                    }
                    if (selected == NULL) {
                        destroy_window(window);
                        free(event);
                        return NULL;
                    }
                }

                destroy_window(window);
            }
            break;

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
    Window *old_focus;

    for (window = first_window; window != NULL; window = window->next) {
        if (window->state.was_ever_mapped) {
            break;
        }
    }

    if (window == NULL) {
        set_notification((FcChar8*) "No managed windows",
                screen->xcb_screen->width_in_pixels / 2,
                screen->xcb_screen->height_in_pixels / 2);
        return NULL;
    }

    xcb_map_window(connection, screen->window_list_window);

    window = focus_window;
    if (window == NULL) {
        if (focus_frame->window == NULL) {
            window = first_window;
        } else {
            window = focus_frame->window;
        }
    }

    old_focus = window;

    window = handle_window_list_events(window, &old_focus);

    xcb_unmap_window(connection, screen->window_list_window);

    if (old_focus != NULL) {
        if (old_focus == focus_window) {
            xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
                focus_window->xcb_window, XCB_CURRENT_TIME);
        } else {
            set_focus_window(old_focus);
        }
    }

    return window;
}
