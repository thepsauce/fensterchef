#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "configuration/structure.h"

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

/* Load the user configuration and make it the current configuration.
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
        unsigned modifiers, unsigned button_index, unsigned flags);

/* Grab the mousebindings so we receive the ButtonPress events for them. */
void grab_configured_buttons(Window window);

/* Get a configured key from key modifiers and a key code.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key(
        struct configuration *configuration,
        unsigned modifiers, KeyCode key_code, unsigned flags);

/* Get a configured key from key modifiers and a key symbol.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key_by_key_symbol(
        struct configuration *configuration,
        unsigned modifiers, KeySym key_symbol, unsigned flags);

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
