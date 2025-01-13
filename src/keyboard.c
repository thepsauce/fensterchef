#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include "action.h"
#include "fensterchef.h"
#include "util.h"

static struct {
    int mod;
    xcb_keysym_t keysym;
    int action;
} key_binds[] = {
    { XCB_MOD_MASK_1, XK_Return, ACTION_START_TERMINAL },
    { XCB_MOD_MASK_1, XK_n, ACTION_NEXT_WINDOW },
    { XCB_MOD_MASK_1, XK_p, ACTION_PREV_WINDOW },
};

static xcb_key_symbols_t *keysyms;

static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode)
{
    return xcb_key_symbols_get_keysym(keysyms, keycode, 0);
}

static xcb_keycode_t *xcb_get_keycodes(xcb_keysym_t keysym)
{
    return xcb_key_symbols_get_keycode(keysyms, keysym);
}

/* grab the keybinds so we receive the keypress events for them */
int setup_keys(void)
{
    xcb_screen_t    *screen;
    xcb_keycode_t   *keycode;
    const uint32_t  modifiers[] = { 0, XCB_MOD_MASK_LOCK };

    keysyms = xcb_key_symbols_alloc(g_dpy);
    if (keysyms == NULL) {
        return 1;
    }
    screen = g_screens[g_screen_no];
    xcb_ungrab_key(g_dpy, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    for (uint32_t i = 0; i < SIZE(key_binds); i++) {
        keycode = xcb_get_keycodes(key_binds[i].keysym);
        for (uint32_t k = 0; keycode[k] != XCB_NO_SYMBOL; k++) {
            for (uint32_t m = 0; m < SIZE(modifiers); m++) {
                xcb_grab_key(g_dpy, 1, screen->root, key_binds[i].mod | modifiers[m],
                        keycode[k], XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            }
        }
        free(keycode);
    }
    return 0;
}

/* handle a key press event */
void handle_key_press(xcb_key_press_event_t *ev)
{
    xcb_keysym_t keysym;

    keysym = xcb_get_keysym(ev->detail);
    for (uint32_t i = 0; i < SIZE(key_binds); i++) {
        if (keysym == key_binds[i].keysym && key_binds[i].mod == ev->state) {
            LOG(stderr, "triggered keybind: %d\n", key_binds[i].action);
            do_action(key_binds[i].action);
            return;
        }
    }
    LOG(stderr, "trash keybind: %d\n", keysym);
}
