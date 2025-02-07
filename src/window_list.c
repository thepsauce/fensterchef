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

/* Render the window list.
 *
 * TODO: add scrolling
 */
static int render_window_list(Window *selected)
{
    uint32_t                 window_count;
    FcChar8                  *marker_char;
    struct text_measure      tm;
    uint32_t                 max_width;
    struct frame             *primary_frame;
    xcb_rectangle_t          rect;
    xcb_render_color_t       bg_color;
    xcb_render_picture_t     pen;

    /* measure the maximum needed width */
    window_count = 0;
    tm.ascent = 12;
    tm.descent = -4;
    max_width = 0;
    for (Window *w = first_window; w != NULL; w = w->next) {
        if (!w->state.was_ever_mapped) {
            continue;
        }

        window_count++;
        measure_text(w->properties.short_name,
                strlen((char*) w->properties.short_name), &tm);
        max_width = MAX(max_width, tm.total_width);
    }

    if (window_count == 0) {
        return 1;
    }

    primary_frame = get_primary_monitor()->frame;
    general_values[0] = primary_frame->x + primary_frame->width - max_width -
        WINDOW_PADDING / 2;
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

    rect.x = 0;
    rect.y = 0;
    rect.width = max_width + WINDOW_PADDING / 2;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    for (Window *w = first_window; w != NULL; w = w->next) {
        if (!w->state.was_ever_mapped) {
            continue;
        }

        bg_color.alpha = 0xff00;
        if (w == selected) {
            pen = screen->stock_objects[STOCK_WHITE_PEN];
            bg_color.red = 0x0000;
            bg_color.green = 0x0000;
            bg_color.blue = 0x0000;
        } else {
            pen = screen->stock_objects[STOCK_BLACK_PEN];
            bg_color.red = 0xff00;
            bg_color.green = 0xff00;
            bg_color.blue = 0xff00;
        }

        marker_char = w->properties.short_name;
        while (isdigit(marker_char[0])) {
            marker_char++;
        }
        marker_char[0] = !w->state.is_visible ? '-' :
                w->state.mode == WINDOW_MODE_POPUP ?
                    (w == focus_window ? '#' : '=') :
                w->state.mode == WINDOW_MODE_FULLSCREEN ?
                    (w == focus_window ? '@' : 'F') :
                w == focus_window ? '*' : '+';

        draw_text(screen->window_list_window, w->properties.short_name,
                strlen((char*) w->properties.short_name), bg_color, &rect,
                pen, WINDOW_PADDING / 2,
                rect.y + tm.ascent + WINDOW_PADDING / 2);
        rect.y += rect.height;
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
