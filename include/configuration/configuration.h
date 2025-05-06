#ifndef CONFIGURATION_H
#define CONFIGURATION_H

/**
 * This is the definition of the configuration structure that holds onto the
 * currently configured options and their value.
 *
 * When adding anything to the configuration, make sure to modify the duplicate
 * and clear functions in configuration.c so all recources are managed properly.
 * This may not be needed if no additional resources need to be allocated.
 */

#include <X11/Xlib.h>

#include "configuration/action.h"
#include "utility/utility.h"

/* button binding */
struct configuration_button {
    /* if this binding is triggered on a release */
    bool is_release;
    /* if the event should pass through to the window the event happened in */
    bool is_transparent;
    /* the button modifiers */
    unsigned modifiers;
    /* the actual mouse button index */
    unsigned index;
    /* the actions to execute */
    struct action_list actions;
};

/* key binding */
struct configuration_key {
    /* if this binding is triggered on a release */
    bool is_release;
    /* the key modifiers */
    unsigned modifiers;
    /* the key symbol, may be `NoSymbol` */
    KeySym key_symbol;
    /* The code of the key, synchronized in `grab_configured_keys()`.  It may
     * also be given explicitly in the configuration.
     */
    KeyCode key_code;
    /* the actions to execute */
    struct action_list actions;
};

/* association between class/instance and window number */
struct configuration_association {
    /* the pattern the instance should match, may be NULL which implies its
     * value is '*'
     */
    _Nullable utf8_t *instance_pattern;
    /* the pattern the class should match */
    utf8_t *class_pattern;
    /* the actions to execute */
    struct action_list actions;
};

/* the currently loaded configuration settings */
extern struct configuration {
    /* the associations that are wanted */
    struct configuration_association *associations;
    /* the number of associations */
    size_t number_of_associations;

    /* the configured buttons */
    struct configuration_button *buttons;
    /* the number of configured buttons */
    size_t number_of_buttons;

    /* the configured keys */
    struct configuration_key *keys;
    /* the number of configured keys */
    size_t number_of_keys;

    /* the cursor used on the root window */
    Cursor root_cursor;
    /* the cursor used for moving a window */
    Cursor moving_cursor;
    /* the cursor used for sizing a window horizontally */
    Cursor horizontal_cursor;
    /* the cursor used for sizing a window vertically */
    Cursor vertical_cursor;
    /* the cursor used for sizing a window */
    Cursor sizing_cursor;

    /* below this point are all simple and shallow settings */
    int _shallow_start;

    /* how many pixels off the edges of windows should be used for resizing */
    int resize_tolerance;

    /* the modifiers to be applied to all bindings */
    unsigned modifiers;
    /* the modifiers to ignore for a binding */
    unsigned modifiers_ignore;

    /* the number the first window gets assigned */
    unsigned first_window_number;

    /* at which percentage to count windows to be overlapped with a monitor */
    unsigned overlap;

    /* whether to automatically create a split when a window is shown */
    bool auto_split;
    /* whether to automatically equalize all frames within the root */
    bool auto_equalize;
    /* whether to fill in empty frames automatically */
    bool auto_fill_void;
    /* whether to remove frames automatically when their inner windows is
     * hidden
     */
    bool auto_remove;
    /* whether to remove frames automatically when they become empty */
    bool auto_remove_void;

    /* the duration in seconds a notification window should linger for */
    unsigned notification_duration;

    /* padding of text within the notification window */
    unsigned text_padding;

    /* width of the border */
    unsigned border_size;
    /* color of the border around the window */
    uint32_t border_color;
    /* color of the border of an unfocused tiling/floating windows */
    uint32_t border_color_active;
    /* color of the border of a focused window */
    uint32_t border_color_focus;
    /* color of the text */
    uint32_t foreground;
    /* color of the background of fensterchef windows */
    uint32_t background;

    /* width of the inner gaps (between frames) */
    int gaps_inner[4];
    /* width of the outer gaps (between frames and monitor boundaries */
    int gaps_outer[4];
} configuration;

/* Clear all resources associated to the given configuration. */
void clear_configuration(struct configuration *configuration);

/* Copy the shallow settings from @configuration into the current configuration.
 */
void copy_configuration_settings(const struct configuration *configuration);

/* Get a key from button modifiers and a button index. */
struct configuration_button *find_configured_button(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, unsigned button_index);

/* Grab the mousebindings so we receive the ButtonPress events for them. */
void grab_configured_buttons(Window window);

/* Get a configured key from key modifiers and a key code.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, KeyCode key_code);

/* Get a configured key from key modifiers and a key symbol.
 *
 * @flags is a combination of `BINDING_FLAG_*` where the transparent flag is
 *        ignored.
 */
struct configuration_key *find_configured_key_by_key_symbol(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, KeySym key_symbol);

/* Grab the key bindings so we receive the KeyPress events for them. */
void grab_configured_keys(void);

#endif
