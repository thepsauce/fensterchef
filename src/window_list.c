#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"

static inline xcb_query_text_extents_reply_t *get_text_extent(
        int32_t num_chars)
{
    xcb_char2b_t                    ch = { .byte1 = 0, .byte2 = 'X' };
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t  *reply;

    cookie = xcb_query_text_extents(g_dpy, g_font, 1, &ch);
    reply = xcb_query_text_extents_reply(g_dpy, cookie, NULL);
    reply->overall_width *= num_chars;
    return reply;
}

void get_window_title(xcb_window_t xcb_window, char *buf, uint32_t max_len)
{
    xcb_get_property_cookie_t   cookie;
    xcb_get_property_reply_t    *reply;
    int                         len;

    buf[0] = '\0';

    cookie = xcb_get_property(g_dpy, 0, xcb_window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, (max_len >> 2) + 1);
    reply = xcb_get_property_reply(g_dpy, cookie, NULL);
    if (reply == NULL) {
        return;
    }
    len = xcb_get_property_value_length(reply);
    len = MIN(max_len, (uint32_t) len);
    memcpy(buf, xcb_get_property_value(reply), len);
    buf[len - 1] = '\0';

    free(reply);
}

static void render_window_list(
        xcb_query_text_extents_reply_t *extents,
        Window *selected)
{
    int32_t         y = 0;
    char            buf[32];
    int             n;
    xcb_gcontext_t  drawing_context, rect_context;
    xcb_rectangle_t rect;

    for (Window *w = g_first_window; w != NULL; w = w->next) {
        n = snprintf(buf, sizeof(buf), "%" PRId32 "%c ", w->number,
                w->focused ? 'f' : w->visible ? 'v' : ' ');
        get_window_title(w->xcb_window, &buf[n], sizeof(buf) - n);
        if (w == selected) {
            drawing_context = g_inverted_context;
            rect_context = g_drawing_context;
        } else {
            drawing_context = g_drawing_context;
            rect_context = g_inverted_context;
        }
        rect.x = 0;
        rect.y = y;
        rect.width = extents->overall_width;
        rect.height = extents->font_ascent + extents->font_descent +
            WINDOW_PADDING;
        xcb_poly_fill_rectangle(g_dpy, g_window_list_window, rect_context,
                1, &rect);
        xcb_image_text_8(g_dpy, strlen(buf), g_window_list_window,
                drawing_context, WINDOW_PADDING / 2,
                y + extents->font_ascent + WINDOW_PADDING / 2, buf);
        y += rect.height;
    }
}

static inline Window *handle_window_list_events(
        xcb_query_text_extents_reply_t *extents,
        Window *selected)
{
    xcb_generic_event_t             *event;
    xcb_key_press_event_t           *key_press;
    xcb_keysym_t                    keysym;

    while (xcb_connection_has_error(g_dpy) == 0) {
        event = xcb_wait_for_event(g_dpy);
        if (event != NULL) {
            switch ((event->response_type & ~0x80)) {
            case XCB_EXPOSE:
                render_window_list(extents, selected);
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
                    render_window_list(extents, selected);
                    break;

                case XK_End:
                    selected = g_first_window;
                    while (selected->next != NULL) {
                        selected = selected->next;
                    }
                    render_window_list(extents, selected);
                    break;

                case XK_k:
                case XK_Up:
                    selected = get_previous_window(selected);
                    render_window_list(extents, selected);
                    break;

                case XK_j:
                case XK_Down:
                    selected = selected->next;
                    if (selected == NULL) {
                        selected = g_first_window;
                    }
                    render_window_list(extents, selected);
                    break;
                }
                break;

            case XCB_BUTTON_PRESS:
                /* TODO: mouse support */
                break;

            default:
                handle_event(event);
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
    uint32_t                        window_count;
    xcb_screen_t                    *screen;
    xcb_query_text_extents_reply_t  *reply;
    Window                          *old_focus;
    Window                          *window;

    window_count = 0;
    for (Window *w = g_first_window; w != NULL; w = w->next) {
        window_count++;
    }

    reply = get_text_extent(32);
    if (reply == NULL) {
        LOG("could not get text extent reply for the window list window\n");
        return NULL;
    }

    screen = g_screens[g_screen_no];

    if (window_count == 0) {
        set_notification("No managed windows\n",
                screen->width_in_pixels / 2, screen->height_in_pixels / 2);
        return NULL;
    }

    /* TODO: show window on current monitor */
    g_values[0] = screen->width_in_pixels - reply->overall_width -
        WINDOW_PADDING / 2;
    g_values[1] = 0;
    g_values[2] = reply->overall_width - WINDOW_PADDING / 2;
    g_values[3] = window_count *
        (reply->font_ascent + reply->font_descent + WINDOW_PADDING);
    g_values[4] = 1;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_window_list_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_map_window(g_dpy, g_window_list_window);

    window = get_focus_window();

    render_window_list(reply, window);

    old_focus = get_focus_window();

    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
            g_window_list_window, XCB_CURRENT_TIME);

    window = handle_window_list_events(reply, window);

    if (old_focus != NULL) {
        xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
                old_focus->xcb_window, XCB_CURRENT_TIME);
    }

    free(reply);

    xcb_unmap_window(g_dpy, g_window_list_window);

    return window;
}
