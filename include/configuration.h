#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>
#include <stdint.h>

#include "action.h"
#include "keymap.h"

/* if the user requested to reload the configuration, handled in `main()` */
extern bool reload_requested;

/* general settings */
struct configuration_general {
    /* currently no members */
    uint32_t unused;
};

/* tiling settings */
struct configuration_tiling {
    /* whether to fill in empty frames automatically */
    bool auto_fill_void;
    /* whether to remove empty frames automatically */
    bool auto_remove_void;
};

/* font settings */
struct configuration_font {
    /* name of the font in fontconfig format */
    uint8_t *name;
};

/* border settings (tiling and popup) */
struct configuration_border {
    /* width of the border */
    uint32_t size;
    /* color of the border of an unfocused window */
    uint32_t color;
    /* color of the border of a focused window */
    uint32_t focus_color;
};

/* gaps settings for the tiling layout */
struct configuration_gaps {
    /* width of the inner gaps (between frames) */
    uint32_t inner;
    /* width of the inner gaps (between frames and monitor boundaries) */
    uint32_t outer;
};

/* notification window settings */
struct configuration_notification {
    /* the duration in seconds a notification window should linger for */
    uint32_t duration;
    /* padding of text within the notification window */
    uint32_t padding;
    /* width of the border */
    uint32_t border_size;
    /* color of the border */
    uint32_t border_color;
    /* color of the text */
    uint32_t foreground;
    /* color of the background */
    uint32_t background;
};

/* keybinding */
struct configuration_key {
    /* the key modifiers */
    uint16_t modifiers;
    /* the key symbol */
    xcb_keysym_t key_symbol;
    /* the actions to execute */
    Action *actions;
    uint32_t number_of_actions;
};

/* keyboard settings */
struct configuration_keyboard {
    /* the modifier key applied for all keys (applied at the parsing step) */
    uint16_t modifiers;
    /* the modifiers to ignore */
    uint16_t ignore_modifiers;
    /* the configured keys */
    struct configuration_key *keys;
    /* the number of configured keys */
    uint32_t number_of_keys;
};

/* the currently loaded configuration */
extern struct configuration {
    /* general settings */
    struct configuration_general general;
    /* general settings */
    struct configuration_tiling tiling;
    /* font settings */
    struct configuration_font font;
    /* border settings */
    struct configuration_border border;
    /* gap settings */
    struct configuration_gaps gaps;
    /* notifiaction window settings */
    struct configuration_notification notification;
    /* keyboard settings / keybindings */
    struct configuration_keyboard keyboard;
} configuration;

/* Puts the keybindings of the default configuration into @configuration without
 * overwriting any keybindings.
 */
void merge_with_default_key_bindings(struct configuration *configuration);

/* Load the default values into the configuration. */
void load_default_configuration(void);

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

/* Grab the keybinds so we receive the keypress events for them. */
void grab_configured_keys(void);

/* Helper function to free resources of a key struct. */
void free_key_actions(struct configuration_key *key);

/* Get a key from key modifiers and a key symbol.
 *
 * @return NULL when that key is not binded.
 */
struct configuration_key *find_configured_key(struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol);

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *configuration);

/* Load the configuration within given file name.
 *
 * @return ERROR if the file could not be read, OK otherwise.
 */
int load_configuration_file(const char *file_name,
        struct configuration *configuration);

#endif
