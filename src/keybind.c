/* This file handles translation of keysyms to keycodes and vise versa.
 *
 * It also handles keybinds.
 */

#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include "action.h"
#include "fensterchef.h"
#include "keybind.h"
#include "keymap.h"
#include "log.h"
#include "screen.h"
#include "util.h"

#define IGNORE_MODIFIER_MASK \
    (XCB_MOD_MASK_LOCK|XCB_MOD_MASK_2|XCB_MOD_MASK_3|XCB_MOD_MASK_5)

/* all keybinds */
static struct {
    /* the key modifier */
    uint16_t modifier;
    /* the key symbol */
    xcb_keysym_t keysym;
    /* the action to execute */
    action_t action;
} key_binds[] = {
    { XCB_MOD_MASK_1, XK_Return, ACTION_START_TERMINAL },
    { XCB_MOD_MASK_1, XK_n, ACTION_NEXT_WINDOW },
    { XCB_MOD_MASK_1, XK_p, ACTION_PREV_WINDOW },

    { XCB_MOD_MASK_1, XK_r, ACTION_REMOVE_FRAME },

    { XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT, XK_space,
        ACTION_CHANGE_WINDOW_STATE },
    { XCB_MOD_MASK_1, XK_space, ACTION_CHANGE_FOCUS },

    { XCB_MOD_MASK_1, XK_f, ACTION_TOGGLE_FULLSCREEN },

    { XCB_MOD_MASK_1, XK_v, ACTION_SPLIT_HORIZONTALLY },
    { XCB_MOD_MASK_1, XK_s, ACTION_SPLIT_VERTICALLY },

    { XCB_MOD_MASK_1, XK_k, ACTION_MOVE_UP },
    { XCB_MOD_MASK_1, XK_h, ACTION_MOVE_LEFT },
    { XCB_MOD_MASK_1, XK_l, ACTION_MOVE_RIGHT },
    { XCB_MOD_MASK_1, XK_j, ACTION_MOVE_DOWN },

    { XCB_MOD_MASK_1, XK_w, ACTION_SHOW_WINDOW_LIST },

    { XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT, XK_e,
        ACTION_QUIT_WM },
};

/* Grab the keybinds so we receive the keypress events for them. */
void init_keybinds(void)
{
    xcb_window_t root;
    xcb_keycode_t *keycode;
    uint16_t ignore_modifiers[] = {
        XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2, XCB_MOD_MASK_3, XCB_MOD_MASK_5
    };
    uint16_t modifiers;

    root = g_screen->xcb_screen->root;

    /* remove all previously grabbed keys so that we can overwrite them */
    xcb_ungrab_key(g_dpy, XCB_GRAB_ANY, root, XCB_MOD_MASK_ANY);

    for (uint32_t i = 0; i < SIZE(key_binds); i++) {
        /* go over all keycodes of a specific key symbol and grab them with
         * needed modifiers
         */
        keycode = get_keycodes(key_binds[i].keysym);
        for (uint32_t j = 0; keycode[j] != XCB_NO_SYMBOL; j++) {
            /* use every possible combination of modifiers we do not care about
             * so that when the user has CAPS LOCK for example, it does not mess
             * with keybinds.
             */
            for (uint32_t k = 0; k < (1 << SIZE(ignore_modifiers)); k++) {

                modifiers = key_binds[i].modifier;

                for (uint32_t l = 0, b = 1; l < SIZE(ignore_modifiers);
                        l++, b <<= 1) {
                    if ((k & b)) {
                        modifiers |= ignore_modifiers[l];
                    }
                }

                xcb_grab_key(g_dpy,
                        1, /* 1 means we specify a specific window for grabbing */
                        root, /* this is the window we grab the key for */
                        modifiers, keycode[j],
                        /* the ASYNC constants are so that event processing
                         * continues normally, otherwise we would need to issue
                         * an AllowEvents request each time we receive KeyPress
                         * events
                         */
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC);
            }
        }
        free(keycode);
    }
}

/* Get an action code from a key press event. */
action_t get_action_bind(xcb_key_press_event_t *event)
{
    xcb_keysym_t keysym;
    uint16_t modifiers;

    /* find a matching key bind (the keysym AND modifier must match up) */
    keysym = get_keysym(event->detail);
    modifiers = (event->state & ~IGNORE_MODIFIER_MASK);
    for (uint32_t i = 0; i < SIZE(key_binds); i++) {
        if (keysym == key_binds[i].keysym &&
                key_binds[i].modifier == modifiers) {
            return key_binds[i].action;
        }
    }

    LOG("trash keybind: %d\n", keysym);
    return ACTION_NULL;
}
