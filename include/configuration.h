#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "bits/configuration_structure.h"

/* the currently loaded configuration */
extern struct configuration configuration;

/* Create a deep copy of @duplicate and put it into itself.
 *
 * For example to create a copy of the currently active configuration:
 * ```C
 * struct configuration duplicate = configuration;
 * duplicate_configuration(&duplicate);
 * // duplicate is now independant of `configuration`
 * ```
 */
void duplicate_configuration(struct configuration *duplicate);

/* Clear the resources given configuration occupies. */
void clear_configuration(struct configuration *configuration);

/* Load the user configuration and merge it into the current configuration.
 *
 * Due to a bug, this function can not be called directly. Use
 * `reload_requested = true` instead, this will then call this function in
 * `next_cycle()`.
 */
void reload_user_configuration(void);

/* Get a key from button modifiers and a button index.
 *
 * Note that this ignores BINDING_FLAG_TRANSPARENT.
 */
struct configuration_button *find_configured_button(
        struct configuration *configuration,
        uint16_t modifiers, xcb_button_t button_index, uint16_t flags);

/* Grab the mousebindings so we receive the ButtonPress events for them. */
void grab_configured_buttons(xcb_window_t window);

/* Get a configured key from key modifiers and a key code.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key_by_code(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keycode_t key_code, uint16_t flags);

/* Get a key from key modifiers and a key symbol.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key_by_symbol(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol, uint16_t flags);

/* Grab the keybindings so we receive the KeyPress events for them. */
void grab_configured_keys(void);

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *configuration);

/* Load a configuration from a string or file.
 *
 * @return ERROR if the file could not be read or a syntax error occured,
 *         OK otherwise.
 */
int load_configuration(const char *string,
        struct configuration *configuration, bool load_from_file);

#endif
