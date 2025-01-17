#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"

static int render_window_list(Window *selected)
{
    uint32_t                    window_count, i;
    struct tile {
        xcb_get_property_cookie_t cookie;
        FcChar8 buf[256];
    }                           *titles = NULL;
    xcb_get_property_reply_t    *reply;
    struct text_measure         tm;
    uint32_t                    max_width = 0;
    xcb_screen_t                *screen;
    xcb_rectangle_t             rect;
    xcb_render_color_t          bg_color;
    xcb_render_picture_t        pen;

    window_count = 0;
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        window_count++;
    }

    if (window_count == 0) {
        return 1;
    }

    RESIZE(titles, window_count);

    i = 0;
    for (Window *w = g_first_window; w != NULL; w = w->next, i++) {
        titles[i].cookie = xcb_get_property(g_dpy, 0, w->xcb_window,
                XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0,
                (sizeof(titles[0].buf) >> 2) + 1);
    }

    i = 0;
    tm.ascent = 12;
    tm.descent = -4;
    for (Window *w = g_first_window; w != NULL; w = w->next, i++) {
        reply = xcb_get_property_reply(g_dpy, titles[i].cookie, NULL);
        snprintf((char*) titles[i].buf, sizeof(titles[i].buf),
                "%" PRId32 "%c%.*s",
                w->number, w->focused ? '*' : w->visible ? '+' : '-',
                reply == NULL ? 0 : xcb_get_property_value_length(reply),
                reply == NULL ? "" : (char*) xcb_get_property_value(reply));
        free(reply);
        measure_text(titles[i].buf, strlen((char*) titles[i].buf), &tm);
        max_width = MAX(max_width, tm.total_width);
    }

    screen = g_screens[g_screen_no];
    /* TODO: show window on current monitor */
    g_values[0] = screen->width_in_pixels - max_width -
        WINDOW_PADDING / 2;
    g_values[1] = 0;
    g_values[2] = max_width + WINDOW_PADDING / 2;
    g_values[3] = window_count * (tm.ascent - tm.descent + WINDOW_PADDING);
    g_values[4] = 1;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
            g_window_list_window, XCB_CURRENT_TIME);

    i = 0;
    rect.x = 0;
    rect.y = 0;
    rect.width = max_width + WINDOW_PADDING / 2;
    rect.height = tm.ascent - tm.descent + WINDOW_PADDING;
    for (Window *w = g_first_window; w != NULL; w = w->next, i++) {
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
        draw_text(g_window_list_window, titles[i].buf,
                strlen((char*) titles[i].buf), bg_color, &rect,
                pen, WINDOW_PADDING / 2,
                rect.y + tm.ascent + WINDOW_PADDING / 2);
        rect.y += rect.height;
    }
    return 0;
}

static inline Window *handle_window_list_events(Window *selected)
{
    xcb_generic_event_t             *event;
    xcb_key_press_event_t           *key_press;
    xcb_keysym_t                    keysym;

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

            case XCB_BUTTON_PRESS:
                /* TODO: mouse support */
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
    xcb_screen_t                    *screen;
    Window                          *old_focus;
    Window                          *window;

    screen = g_screens[g_screen_no];
    if (g_first_window == NULL) {
        set_notification((FcChar8*) "No managed windows",
                screen->width_in_pixels / 2, screen->height_in_pixels / 2);
        return NULL;
    }

    xcb_map_window(g_dpy, g_window_list_window);

    window = get_focus_window();

    old_focus = window;

    render_window_list(window);

    window = handle_window_list_events(window);

    if (old_focus != NULL) {
        xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                old_focus->xcb_window, XCB_CURRENT_TIME);
    }

    xcb_unmap_window(g_dpy, g_window_list_window);

    return window;
}
