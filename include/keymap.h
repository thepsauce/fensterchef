#ifndef KEYMAP_H
#define KEYMAP_H

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Initializes the key symbol table so the below functions can be used. */
int initialize_keymap(void);

/* Refresh the keymap if a mapping notify event arrives. */
void refresh_keymap(xcb_mapping_notify_event_t *event);

/* Get a keysym from a keycode. */
xcb_keysym_t get_keysym(xcb_keycode_t keycode);

/* Get a list of keycodes from a keysym.
 * The caller needs to free this list.
 */
xcb_keycode_t *get_keycodes(xcb_keysym_t keysym);

#endif
