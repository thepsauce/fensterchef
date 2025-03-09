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
 * `main()`.
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

/* Get a key from key modifiers and a key symbol.
 *
 * Note that this ignores BINDING_FLAG_TRANSPARENT.
 */
struct configuration_key *find_configured_key(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol, uint16_t flags);

/* Grab the keybindings so we receive the KeyPress events for them. */
void grab_configured_keys(void);

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *configuration);

/* Load the configuration within given file name.
 *
 * @return ERROR if the file could not be read, OK otherwise.
 */
int load_configuration_file(const char *file_name,
        struct configuration *configuration);

#endif
