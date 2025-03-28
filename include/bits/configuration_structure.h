#ifndef CONFIGURATION_STRUCTURE_H
#define CONFIGURATION_STRUCTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "action.h"
#include "expression.h"
#include "keymap.h"
#include "utf8.h"
#include "utility.h"

/**
 * When adding anything to the configuration, make sure to modify the duplicate
 * and clear functions in configuration.c so all recources are managment
 * properly.
 */

/* if the binding should be for a release event */
#define BINDING_FLAG_RELEASE 0x1
/* if the event should be passed down to the window */
#define BINDING_FLAG_TRANSPARENT 0x2

/* button binding */
struct configuration_button {
    /* the button modifiers */
    uint16_t modifiers;
    /* additional flags */
    uint16_t flags;
    /* the actual mouse button index */
    xcb_button_t index;
    /* the expression to evaluate */
    Expression expression;
};

/* key binding */
struct configuration_key {
    /* the key modifiers */
    uint16_t modifiers;
    /* additional flags */
    uint16_t flags;
    /* the key symbol */
    xcb_keysym_t key_symbol;
    /* the code of the key, used when `key_symbol` is NoSymbol */
    xcb_keycode_t key_code;
    /* the expression to evaluate */
    Expression expression;
};

/* association between class/instance and window number */
struct configuration_association {
    /* the window number */
    uint32_t number;
    /* the pattern the instance should match */
    utf8_t *instance_pattern;
    /* the pattern the class should match */
    utf8_t *class_pattern;
    /* the expression to evaluate */
    Expression expression;
};

/**
 * Do not touch anything below this point, it is business of the code generator
 * script.
 */

/*< START OF CONFIGURATION >*/

/* startup settings */
struct configuration_startup {
    /* the expression to evaluate on startup */
    Expression expression;
};

/* general settings */
struct configuration_general {
    /* at which percentage to consider a window to be overlapped with a monitor */
    int32_t overlap_percentage;
    /* the name of the cursor used on the root window */
    int32_t root_cursor;
    /* the name of the cursor used for moving a window */
    int32_t moving_cursor;
    /* the name of the cursor used for sizing a window horizontally */
    int32_t horizontal_cursor;
    /* the name of the cursor used for sizing a window vertically */
    int32_t vertical_cursor;
    /* the name of the cursor used for sizing a window */
    int32_t sizing_cursor;
};

/* assignment settings */
struct configuration_assignment {
    /* the number the first window gets assigned */
    int32_t first_window_number;
    /* the associations that are wanted */
    struct configuration_association *associations;
    /* the number of associations */
    uint32_t number_of_associations;
};

/* tiling settings */
struct configuration_tiling {
    /* whether to automatically create a split when a window is shown */
    int32_t auto_split;
    /* whether to automatically equalize all frames within the root */
    int32_t auto_equalize;
    /* whether to fill in empty frames automatically */
    int32_t auto_fill_void;
    /* whether to remove frames automatically when their inner windows is
     * hidden
     */
    int32_t auto_remove;
    /* whether to remove frames automatically when they become empty */
    int32_t auto_remove_void;
};

/* font settings */
struct configuration_font {
    /* whether to use the core font instead of the better font rendering */
    int32_t use_core_font;
    /* name of the font in fontconfig format */
    utf8_t *name;
};

/* border settings */
struct configuration_border {
    /* width of the border around the windows */
    int32_t size;
    /* color of the border of an unfocused window */
    int32_t color;
    /* color of the border of an unfocused tiling window */
    int32_t active_color;
    /* color of the border of a focused window */
    int32_t focus_color;
};

/* gaps settings */
struct configuration_gaps {
    /* width of the inner gaps (between frames) */
    int32_t inner[4];
    /* width of the outer gaps (between frames and monitor boundaries */
    int32_t outer[4];
};

/* notification settings */
struct configuration_notification {
    /* the duration in seconds a notification window should linger for */
    int32_t duration;
    /* padding of text within the notification window */
    int32_t padding;
    /* width of the border */
    int32_t border_size;
    /* color of the border around the window */
    int32_t border_color;
    /* color of the text */
    int32_t foreground;
    /* color of the background */
    int32_t background;
};

/* mouse settings */
struct configuration_mouse {
    /* how many pixels off the edges of windows should be used for resizing */
    int32_t resize_tolerance;
    /* the modifier key for all buttons (applied at the parsing step) */
    int32_t modifiers;
    /* the modifiers to ignore for a mouse binding */
    int32_t ignore_modifiers;
    /* the configured buttons */
    struct configuration_button *buttons;
    /* the number of configured buttons */
    uint32_t number_of_buttons;
};

/* keyboard settings */
struct configuration_keyboard {
    /* the modifier key for all keys (applied at the parsing step) */
    int32_t modifiers;
    /* the modifiers to ignore for a key binding */
    int32_t ignore_modifiers;
    /* the configured keys */
    struct configuration_key *keys;
    /* the number of configured keys */
    uint32_t number_of_keys;
};

/* configuration settings */
struct configuration {
    /* startup settings */
    struct configuration_startup startup;
    /* general settings */
    struct configuration_general general;
    /* assignment settings */
    struct configuration_assignment assignment;
    /* tiling settings */
    struct configuration_tiling tiling;
    /* font settings */
    struct configuration_font font;
    /* border settings */
    struct configuration_border border;
    /* gaps settings */
    struct configuration_gaps gaps;
    /* notification settings */
    struct configuration_notification notification;
    /* mouse settings */
    struct configuration_mouse mouse;
    /* keyboard settings */
    struct configuration_keyboard keyboard;
};

/*< END OF CONFIGURATION >*/

#endif
