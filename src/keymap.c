#include "fensterchef.h"
#include "keybind.h"
#include "keymap.h"

/* symbol translation table */
static xcb_key_symbols_t *keysyms;

/* Initializes the keymap so the below functions can be used. */
int init_keymap(void)
{
    keysyms = xcb_key_symbols_alloc(g_dpy);
    if (keysyms == NULL) {
        return 1;
    }
    return 0;
}

/* Refresh the keymap if a mapping notify event arrives. */
void refresh_keymap(xcb_mapping_notify_event_t *event)
{
    (void) xcb_refresh_keyboard_mapping(keysyms, event);
    /* regrab all keys */
    init_keybinds();
}

/* Get a keysym from a keycode. */
xcb_keysym_t get_keysym(xcb_keycode_t keycode)
{
    return xcb_key_symbols_get_keysym(keysyms, keycode, 0);
}

/* Get a list of keycodes from a keysym.
 * The caller needs to free this list.
 */
xcb_keycode_t *get_keycodes(xcb_keysym_t keysym)
{
    return xcb_key_symbols_get_keycode(keysyms, keysym);
}
