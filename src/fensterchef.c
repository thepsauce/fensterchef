#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <X11/keysym.h>

#include <stdio.h>

#include "action.h"
#include "fensterchef.h"
#include "xalloc.h"
#include "util.h"

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

static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode)
{
    xcb_key_symbols_t *keysyms;
    xcb_keysym_t       keysym;

    keysyms = xcb_key_symbols_alloc(g_dpy);
    if (keysyms == NULL) {
        return 0;
    }
    keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
    xcb_key_symbols_free(keysyms);
    return keysym;
}

static xcb_keycode_t *xcb_get_keycodes(xcb_keysym_t keysym)
{
    xcb_key_symbols_t   *keysyms;
    xcb_keycode_t       *keycode;

    keysyms = xcb_key_symbols_alloc(g_dpy);
    if (keysyms == NULL) {
        return NULL;
    }
    keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
    xcb_key_symbols_free(keysyms);
    return keycode;
}

static struct {
    int mod;
    xcb_keysym_t keysym;
    int action;
} keys[] = {
    { XCB_MOD_MASK_1, XK_Return, ACTION_START_TERMINAL },
};

/* grab the keybinds so we receive the keypress events for them */
void grab_keys(void)
{
    xcb_screen_t    *screen;
    xcb_keycode_t   *keycode;
    uint32_t        modifiers[] = { 0, XCB_MOD_MASK_LOCK };

    screen = g_screens[g_screen_no];
    xcb_ungrab_key(g_dpy, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    for (uint32_t i = 0; i < SIZE(keys); i++) {
        keycode = xcb_get_keycodes(keys[i].keysym);
        for (uint32_t k = 0; keycode[k] != XCB_NO_SYMBOL; k++) {
            for (uint32_t m = 0; m < SIZE(modifiers); m++) {
                xcb_grab_key(g_dpy, 1, screen->root, keys[i].mod | modifiers[m],
                        keycode[k], XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            }
        }
        free(keycode);
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
    xcb_flush(g_dpy);
    /*xcb_grab_button(g_dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, MOD1);
    xcb_grab_button(g_dpy, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 3, MOD1);*/
    xcb_flush(g_dpy);
    return 0;
}

/* focuses a particular window so that it receives keyboard input */
void set_focus_window(xcb_window_t win)
{
    xcb_set_input_focus(g_dpy, XCB_INPUT_FOCUS_POINTER_ROOT, win,
            XCB_CURRENT_TIME);
}

/* handle the mapping of a new window */
void accept_new_window(xcb_window_t win)
{
    xcb_screen_t *screen;

    screen = g_screens[g_screen_no];
    xcb_map_window(g_dpy, win);
    g_values[0] = 0;
    g_values[1] = 0;
    g_values[2] = screen->width_in_pixels;
    g_values[3] = screen->height_in_pixels;
    g_values[4] = 0;
    xcb_configure_window(g_dpy, win, XCB_CONFIG_WINDOW_X |
        XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
        XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, g_values);
    g_values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_dpy, win,
        XCB_CW_EVENT_MASK, g_values);
    set_focus_window(win);
    xcb_flush(g_dpy);
}

static void handle_key_press(xcb_key_press_event_t *ev)
{
    xcb_keysym_t           keysym;

    keysym = xcb_get_keysym(ev->detail);
    for (uint32_t i = 0; i < SIZE(keys); i++) {
        if (keysym == keys[i].keysym && keys[i].mod == ev->state) {
            LOG(stderr, "triggered keybind: %d\n", keys[i].action);
            do_action(keys[i].action);
        }
    }
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
