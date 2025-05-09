#ifndef BINDING_H
#define BINDING_H

/**
 * User set button and key bindings.  This also includes caching and
 * synchronization functions to grab the keys.
 */

#include <X11/X.h>

#include <stdbool.h>

#include "action.h"
#include "bits/binding.h"

/* Set the modifiers to add to the ones passed into the set binding function. */
void set_additional_modifiers(unsigned modifiers);

/* Set the modifiers to ignore. */
void set_ignored_modifiers(unsigned modifiers);

/* Set the specified button binding. */
void set_button_binding(const struct button_binding *button_binding);

/* Run the specified key binding. */
void run_button_binding(Time event_time, bool is_release,
        unsigned modifiers, button_t button);

/* Clear a button binding. */
void clear_button_binding(bool is_release, unsigned modifiers, button_t button);

/* Clear all configured button bindings. */
void clear_button_bindings(void);

/* Set the specified key binding. */
void set_key_binding(const struct key_binding *key_binding);

/* Run the specified key binding. */
void run_key_binding(bool is_release, unsigned modifiers, KeyCode key_code);

/* Clear a key binding. */
void clear_key_binding(bool is_release, unsigned modiifiers, KeyCode key_code);

/* Clear all configured key bindings. */
void clear_key_bindings(void);

/* Resolve all key symbols in case the mapping changed. */
void resolve_all_key_symbols(void);

/* Grab the button bindings for a window so we receive MousePress/MouseRelease
 * events for it.
 */
void grab_configured_buttons(Window window);

/* Grab the key bindings so we receive KeyPress/KeyRelease events for them. */
void grab_configured_keys(void);

#endif
