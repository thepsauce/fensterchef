#include "fensterchef.h"
#include "keymap.h"

/* symbol translation table */
static xcb_key_symbols_t *keysyms;

int init_keymap(void)
{
    keysyms = xcb_key_symbols_alloc(g_dpy);
    if (keysyms == NULL) {
        return 1;
    }
    return 0;
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
