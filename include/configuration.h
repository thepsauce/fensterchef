#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <fontconfig/fontconfig.h>

#include <stdint.h>

#include "action.h"
#include "keymap.h"

struct configuration_font {
    /* name of the font in fontconfig format */
    FcChar8 *name;
};

struct configuration_border {
    /* width of the border */
    uint32_t size;
    /* color of the border of an unfocused window */
    uint32_t color;
    /* color of the border of a focused window */
    uint32_t focus_color;
};

struct configuration_gaps {
    /* width of a gap */
    uint32_t size;
};

struct configuration_key {
    /* the key modifiers */
    uint16_t modifiers;
    /* the key symbol */
    xcb_keysym_t key_symbol;
    /* the action to execute */
    action_t action;
};

struct configuration_keyboard {
    /* the modifier key applied for all keys */
    uint16_t main_modifiers;
    /* the modifiers to ignore */
    uint16_t ignore_modifiers;
    /* the configured keys */
    struct configuration_key *keys;
    /* the number of configured keys */
    uint32_t number_of_keys;
};

/* the currently loaded configuration */
extern struct configuration {
    /* font settings */
    struct configuration_font font;
    /* border settings */
    struct configuration_border border;
    /* gap settings */
    struct configuration_border gaps;
    /* keyboard settings / key binds */
    struct configuration_keyboard keyboard;
} configuration;

/* Load the default values into the configuration. */
void load_default_configuration(void);

/* Load the user configuration and merge it into the current configuration. */
void reload_user_configuration(void);

/* Grab the keybinds so we receive the keypress events for them. */
void init_configured_keys(void);

/* Get an action value from key modifiers and a key symbol.
 *
 * @return ACTION_NULL if the key is not configured.
 */
action_t find_configured_action(uint16_t modifiers, xcb_keysym_t key_symbol);

/* describes what parts of the configuration to merge */
typedef enum {
    MERGE_NONE = 0,

    MERGE_FONT_NAME = (1 << 0),

    MERGE_BORDER_SIZE = (1 << 1),
    MERGE_BORDER_COLOR = (1 << 2),
    MERGE_BORDER_FOCUS_COLOR = (1 << 3),

    MERGE_GAPS_SIZE = (1 << 4),

    MERGE_KEYBOARD_MAIN_MODIFIERS = (1 << 5),
    MERGE_KEYBOARD_IGNORE_MODIFIERS = (1 << 6),
    MERGE_KEYBOARD_KEYS = (1 << 7),
} merge_t;

/* Merges given configuration into the current configuration. */
void merge_configuration(struct configuration *configuration, merge_t merge);

/* Load the configuration within given file.
 *
 * @return ERROR if the file could not be read, OK otherwise.
 */
int load_configuration_file(const char *file_name,
        struct configuration *configuration, merge_t *merge);

#endif
