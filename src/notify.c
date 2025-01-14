#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "notify.h"
#include "log.h"
#include "util.h"

/* the notification window */
xcb_window_t            g_notification_window;

/* context used for drawing */
static xcb_gcontext_t   drawing_context;

/* the font used for drawing */
static xcb_font_t       font;

/* Handle an incoming alarm. */
static void alarm_handler(int sig)
{
    (void) sig;
    LOG("triggered alarm: hiding notification window\n");
    xcb_unmap_window(g_dpy, g_notification_window);
    xcb_flush(g_dpy);
}

/* Initialize the notification window. */
int init_notification(void)
{
    xcb_screen_t        *screen;
    xcb_generic_error_t *error;
    const char          *font_name = "-misc-fixed-*";

    signal(SIGALRM, alarm_handler);

    screen = g_screens[g_screen_no];
    g_notification_window = xcb_generate_id(g_dpy);
    error = xcb_request_check(g_dpy, xcb_create_window_checked(g_dpy,
                XCB_COPY_FROM_PARENT, g_notification_window,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                0, NULL));
    if (error != NULL) {
        LOG("could not create notification window\n");
        return 1;
    }

    drawing_context = xcb_generate_id(g_dpy);

    font = xcb_generate_id(g_dpy);
    error = xcb_request_check(g_dpy, xcb_open_font_checked(g_dpy,
                font, strlen(font_name), font_name));
    if (error != NULL) {
        LOG("could not create notification window font\n");
        return 1;
    }

    g_values[0] = screen->black_pixel;
    g_values[1] = screen->white_pixel;
    g_values[2] = font;
    error = xcb_request_check(g_dpy,
            xcb_create_gc_checked(g_dpy, drawing_context, screen->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, g_values));
    if (error != NULL) {
        LOG("could not create graphics context for notifications\n");
        return 1;
    }
    return 0;
}

/* Show the notification window with given message at given coordinates. */
void set_notification(const char *msg, int32_t x, int32_t y)
{
    size_t                          msg_len;
    xcb_char2b_t                    *xcb_str = NULL;
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t  *reply;
    uint32_t                        w, h;

    msg_len = strlen(msg);
    RESIZE(xcb_str, msg_len);
    for (size_t i = 0; i < msg_len; i++) {
        xcb_str[i].byte1 = 0;
        xcb_str[i].byte2 = msg[i];
    }
    cookie = xcb_query_text_extents(g_dpy, font, msg_len, xcb_str);
    free(xcb_str);

    reply = xcb_query_text_extents_reply(g_dpy, cookie, NULL);
    if (reply == NULL) {
        LOG("could not get text extent reply for the notification window\n");
        return;
    }

    w = reply->overall_width;
    h = reply->font_ascent + reply->font_descent;
    x -= w / 2;
    y -= h / 2;

    g_values[0] = x;
    g_values[1] = y;
    g_values[2] = w;
    g_values[3] = h;
    g_values[4] = 0;
    g_values[5] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_dpy, g_notification_window,
            XCB_CONFIG_SIZE | XCB_CONFIG_WINDOW_STACK_MODE, g_values);

    xcb_map_window(g_dpy, g_notification_window);

    xcb_image_text_8(g_dpy, strlen(msg), g_notification_window, drawing_context,
            0, reply->font_ascent, msg);

    alarm(3);

    free(reply);
}
