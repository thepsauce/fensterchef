#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <xcb/xcb.h>

/* Grab all keys so we receive the keypress events for them. */
int init_keybinds(void);

/* Get an action code from a key press event (keybind).
 *
 * @return the action index or ACTION_NULL if the bind does not exist.
 */
action_t get_action_bind(xcb_key_press_event_t *event);

#endif
