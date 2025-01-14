#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_atom.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "util.h"
#include "xalloc.h"

/* event mask for the root window */
#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | \
                         XCB_EVENT_MASK_BUTTON_PRESS | \
                         /* when the user adds a monitor (e.g. video
                          * projector), the root window gets a
                          * ConfigureNotify */ \
                         XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | \
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
uint32_t            g_values[6];

const char          *g_atom_names[ATOM_COUNT] = {
    [WM_PROTOCOLS]      = "WM_PROTOCOLS",
    [WM_DELETE_WINDOW]  = "WM_DELETE_WINDOW",

    [NET_SUPPORTED]     = "NET_SUPPORED",
    [NET_FULLSCREEN]    = "NET_FULLSCREEN",
    [NET_WM_STATE]      = "NET_WM_STATE",
    [NET_ACTIVE]        = "NET_ACTIVE",
};

/* all interned atoms */
xcb_atom_t          g_atoms[ATOM_COUNT];

/* Init the connection to xcb. */
void init_connection(void)
{
    g_dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(g_dpy) > 0) {
        LOG("could not create xcb connection\n");
        exit(1);
    }
}

/* Initialize the screens. */
void init_screens(void)
{
    xcb_screen_iterator_t   iter;

    iter = xcb_setup_roots_iterator(xcb_get_setup(g_dpy));
    RESIZE(g_screens, iter.rem);
    g_screen_count = 0;
    for (; iter.rem > 0; xcb_screen_next(&iter)) {
        g_screens[g_screen_count++] = iter.data;
    }
}

/* Subscribe to event substructe redirecting so that we receive map requests. */
int take_control(void)
{
    xcb_screen_t                *screen;
    xcb_generic_error_t         *error;
    xcb_intern_atom_cookie_t    cookies[ATOM_COUNT];
    xcb_intern_atom_reply_t     *reply;

    for (uint32_t i = 0; i < ATOM_COUNT; i++) {
        cookies[i] = xcb_intern_atom(g_dpy, 0,
                strlen(g_atom_names[i]), g_atom_names[i]);
    }

    for (uint32_t i = 0; i < ATOM_COUNT; i++) {
        reply = xcb_intern_atom_reply(g_dpy, cookies[i], NULL);
        if (reply == NULL) {
            LOG("failed interning %s\n", g_atom_names[i]);
        } else {
            LOG("interned atom %s : %" PRId32 "\n", g_atom_names[i],
                    reply->atom);
            g_atoms[i] = reply->atom;
            free(reply);
        }
    }

    screen = g_screens[g_screen_no];

    xcb_change_property(g_dpy, XCB_PROP_MODE_REPLACE, screen->root,
            g_atoms[NET_SUPPORTED], XCB_ATOM_ATOM, 32,
            ATOM_COUNT - FIRST_NET_ATOM, &g_atoms[FIRST_NET_ATOM]);

    g_values[0] = ROOT_EVENT_MASK;
    error = xcb_request_check(g_dpy,
            xcb_change_window_attributes_checked(g_dpy, screen->root,
                XCB_CW_EVENT_MASK, g_values));
    return error != NULL;
}

/* Handle the mapping of a new window. */
void accept_new_window(xcb_window_t win)
{
    Window *window;

    window = create_window(win);
    if (g_cur_frame->window != NULL) {
        hide_window(g_cur_frame->window);
    }
    g_cur_frame->window = window;
    show_window(window);
    set_focus_window(window);
}

/* Handle the next event xcb has. */
void handle_next_event(void)
{
    xcb_generic_event_t             *event;
    xcb_configure_request_event_t   *request_event;
    Window                          *window;
    Frame                           *frame;

    if (xcb_connection_has_error(g_dpy) > 0) {
        /* quit because the connection is broken */
        g_running = 0;
        return;
    }

    event = xcb_wait_for_event(g_dpy);
    if (event != NULL) {
#ifdef DEBUG
        log_event(event, g_log_file);
        fputc('\n', g_log_file);
#endif
        switch ((event->response_type & ~0x80)) {
        case XCB_MAP_REQUEST:
            accept_new_window(((xcb_map_request_event_t*) event)->window);
            break;

        case XCB_UNMAP_NOTIFY:
            window = get_window_of_xcb_window(
                    ((xcb_unmap_notify_event_t*) event)->window);
            if (window != NULL && window->visible) {
                window->visible = 0;
                frame = get_frame_of_window(window);
                window = get_next_hidden_window(window);
                frame->window = window;
                if (window != NULL) {
                    show_window(window);
                }
            }
            break;

        case XCB_DESTROY_NOTIFY:
            window = get_window_of_xcb_window(
                    ((xcb_unmap_notify_event_t*) event)->window);
            if (window != NULL) {
                destroy_window(window);
            }
            break;

        case XCB_CONFIGURE_REQUEST:
            request_event = (xcb_configure_request_event_t*) event;
            g_values[0] = request_event->x;
            g_values[1] = request_event->y;
            g_values[2] = request_event->width;
            g_values[3] = request_event->height;
            g_values[4] = request_event->border_width;
            xcb_configure_window(g_dpy, request_event->window,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    g_values);
            break;

        case XCB_KEY_PRESS:
            handle_key_press((xcb_key_press_event_t*) event);
            break;
        }
        free(event);
    }
}

/* Close the connection to the xcb server. */
void close_connection(void)
{
    xcb_disconnect(g_dpy);
}
