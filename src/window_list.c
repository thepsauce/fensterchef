#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"

/* Render the window list.
 *
 * First, this measures the maximum length a title needs and then configures
 * the window size to fit that and focuses the window.
 *
 * Lastly, the titles are drawn by first replacing the indicator character
 * within the short title of the window and then rendering that text.
 * The selected window is highlighted with inverted colors.
 *
 * TODO: add scrolling
 */
static int render_window_list(Window *selected)
{
    uint32_t                window_count;
    FcChar8                 *marker_char;
    struct text_measure     tm;
    uint32_t                max_width;
    xcb_screen_t            *screen;
    xcb_rectangle_t         rect;
    xcb_render_color_t      bg_color;
    xcb_render_picture_t    pen;

    window_count = 0;
    tm.ascent = 12;
    tm.descent = -4;
    max_width = 0;
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        window_count++;
        measure_text(w->short_title, strlen((char*) w->short_title), &tm);
        max_width = MAX(max_width, tm.total_width);
    }

    if (window_count == 0) {
        return 1;
    }

    screen = SCREEN(g_screen_no);
    /* TODO: show window on current monitor */
    g_values[0] = screen->width_in_pixels - max_width -
        WINDOW_PADDING / 2;
    g_values[1] = 0;
    g_values[2] = max_width + WINDOW_PADDING / 2;
    g_values[3] = window_count * (tm.ascent - tm.descent + WINDOW_PADDING);
    g_values[4] = 1;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_BORDER_WIDTH |
            XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
            g_window_list_window, XCB_CURRENT_TIME);

    rect.x = 0;
    rect.y = 0;
    rect.width = max_width + WINDOW_PADDING / 2;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        bg_color.alpha = 0xff00;
        if (w == selected) {
            pen = g_stock_objects[STOCK_WHITE_PEN];
            bg_color.red = 0x0000;
            bg_color.green = 0x0000;
            bg_color.blue = 0x0000;
        } else {
            pen = g_stock_objects[STOCK_BLACK_PEN];
            bg_color.red = 0xff00;
            bg_color.green = 0xff00;
            bg_color.blue = 0xff00;
        }

        marker_char = w->short_title;
        while (isdigit(marker_char[0])) {
            marker_char++;
        }
        marker_char[0] = w->state == WINDOW_STATE_POPUP ?
                    (w->focused ? '#' : '=') :
                w->state == WINDOW_STATE_FULLSCREEN ?
                    (w->focused ? '@' : 'F') :
                w->focused ? '*' :
                w->state == WINDOW_STATE_SHOWN ? '+' : '-',

        draw_text(g_window_list_window, w->short_title,
                strlen((char*) w->short_title), bg_color, &rect,
                pen, WINDOW_PADDING / 2,
                rect.y + tm.ascent + WINDOW_PADDING / 2);
        rect.y += rect.height;
    }
    return 0;
}

static inline Window *handle_window_list_events(Window *selected,
        Window **p_old_focus)
{
    xcb_generic_event_t     *event;
    xcb_key_press_event_t   *key_press;
    xcb_keysym_t            keysym;
    Window                  *window;

    while (xcb_connection_has_error(g_dpy) == 0) {
        event = xcb_wait_for_event(g_dpy);
        if (event != NULL) {
            switch ((event->response_type & ~0x80)) {
            case XCB_EXPOSE:
                render_window_list(selected);
                break;

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
                    selected = g_first_window;
                    break;

                case XK_End:
                    selected = g_first_window;
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
                        selected = g_first_window;
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
                            selected = g_first_window;
                        }
                        if (selected == NULL) {
                            destroy_window(window);
                            return NULL;
                        }
                    }

                    destroy_window(window);
                }
                break;

            default:
                handle_event(event);
            }
            if (render_window_list(selected) != 0) {
                return NULL;
            }
            free(event);
            xcb_flush(g_dpy);
        }
    }
    return NULL;
}

/* Shows a windows list where the user can select a window from. */
Window *select_window_from_list(void)
{
    xcb_screen_t    *screen;
    Window          *old_focus;
    Window          *window;

    screen = SCREEN(g_screen_no);
    if (g_first_window == NULL) {
        set_notification((FcChar8*) "No managed windows",
                screen->width_in_pixels / 2, screen->height_in_pixels / 2);
        return NULL;
    }

    xcb_map_window(g_dpy, g_window_list_window);

    window = get_focus_window();
    if (window == NULL) {
        if (g_frames[g_cur_frame].window == NULL) {
            window = g_first_window;
        } else {
            window = g_frames[g_cur_frame].window;
        }
    }

    old_focus = window;

    render_window_list(window);

    window = handle_window_list_events(window, &old_focus);

    if (old_focus != NULL) {
        old_focus->focused = 1;

        xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                old_focus->xcb_window, XCB_CURRENT_TIME);
    }

    xcb_unmap_window(g_dpy, g_window_list_window);

    return window;
}
