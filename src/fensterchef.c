#include <stdlib.h>
#include <stdio.h>

#include "frame.h"
#include "fensterchef.h"
#include "xalloc.h"
#include "tilling.h"

#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | \
                         XCB_EVENT_MASK_BUTTON_PRESS | \
                         /* when the user adds a monitor (e.g. video
                          * projector), the root window gets a
                          * ConfigureNotify */ \
                         XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                         XCB_EVENT_MASK_POINTER_MOTION | \
                         XCB_EVENT_MASK_PROPERTY_CHANGE | \
                         XCB_EVENT_MASK_FOCUS_CHANGE | \
                         XCB_EVENT_MASK_ENTER_WINDOW)

/* xcb server connection */
xcb_connection_t    *g_dpy;

/* screens */
xcb_screen_t        **g_screens;
uint32_t            g_screen_count;
/* the screen being used */
uint32_t            g_screen_no;

/* 1 while the window manager is running */
int                 g_running;

/* general purpose values */
uint32_t            g_values[5];

/* init the connection to xcb */
void init_connection(void)
{
    g_dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(g_dpy) > 0) {
        LOG(stderr, "could not create xcb connection\n");
        exit(1);
    }
}

/* log the screens information to a file */
void log_screens(FILE *fp)
{
    xcb_screen_t            *screen;

    LOG(fp, "Have %u screen(s):\n", g_screen_count);
    for (uint32_t i = 0; i < g_screen_count; i++) {
        screen = g_screens[i];
        LOG(fp, "Screen %u ; %u:\n", i, screen->root);
        LOG(fp, "  width.........: %u\n", screen->width_in_pixels);
        LOG(fp, "  height........: %u\n", screen->height_in_pixels);
        LOG(fp, "  white pixel...: %u\n", screen->white_pixel);
        LOG(fp, "  black pixel...: %u\n", screen->black_pixel);
        LOG(fp, "\n");
    }
}

/* initialize the screens */
void init_screens(void)
{
    xcb_screen_iterator_t   iter;

    iter = xcb_setup_roots_iterator(xcb_get_setup(g_dpy));
    g_screens = xmalloc(sizeof(*g_screens) * iter.rem);
    g_screen_count = 0;
    for (; iter.rem > 0; xcb_screen_next(&iter)) {
        g_screens[g_screen_count++] = iter.data;
    }
}

/* subscribe to event substructe redirecting so that we receive map/unmap
 * requests */
int take_control(void)
{
    xcb_screen_t        *screen;
    xcb_generic_error_t *error;

    screen = g_screens[g_screen_no];
    g_values[0] = ROOT_EVENT_MASK;
    error = xcb_request_check(g_dpy, xcb_change_window_attributes_checked(g_dpy, screen->root,
            XCB_CW_EVENT_MASK, g_values));
    if (error != NULL) {
        return 1;
    }
    //xcb_ungrab_key(g_dpy, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    /*xcb_grab_button(g_dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, MOD1);
    xcb_grab_button(g_dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 3, MOD1);*/
    xcb_flush(g_dpy);
    return 0;
}

/* handle the mapping of a new window */
void accept_new_window(xcb_window_t win)
{
    Window *window;

    window = create_window(win);
    if (g_frames[g_cur_frame].window != NULL) {
        hide_window(g_frames[g_cur_frame].window);
    }
    g_frames[g_cur_frame].window = window;
    show_window(window);
    set_focus_window(window);
}

/* declare this, it is implemented in event.c */
void log_event(xcb_generic_event_t *ev, FILE *fp);

/* handle the next event xcb has */
void handle_event(void)
{
    xcb_generic_event_t *ev;

    if (xcb_connection_has_error(g_dpy) > 0) {
        /* quit because the connection is broken */
        g_running = 0;
        return;
    }
    ev = xcb_wait_for_event(g_dpy);
    if (ev != NULL) {
#ifdef DEBUG
        log_event(ev, stderr);
        fputc('\n', stderr);
#endif
        switch ((ev->response_type & ~0x80)) {
        case XCB_MAP_REQUEST:
            accept_new_window(((xcb_map_request_event_t*) ev)->window);
            break;

        case XCB_KEY_PRESS:
            handle_key_press((xcb_key_press_event_t*) ev);
            break;
        }
        free(ev);
    }
}

/* close the connection to the xcb server */
void close_connection(void)
{
    xcb_disconnect(g_dpy);
}
