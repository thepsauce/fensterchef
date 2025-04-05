#ifndef CONFIGURATION_STRUCTURE_H
#define CONFIGURATION_STRUCTURE_H

#include "configuration/action.h"
#include "configuration/expression.h"
#include "utility.h"

/**
 * When adding anything to the configuration, make sure to modify the duplicate
 * and clear functions in configuration.c so all recources are managed properly.
 */

/* if the binding should be for a release event */
#define BINDING_FLAG_RELEASE 0x1
/* if the event should be passed down to the window */
#define BINDING_FLAG_TRANSPARENT 0x2

/* button binding */
struct configuration_button {
    /* the button modifiers */
    unsigned modifiers;
    /* additional flags */
    unsigned flags;
    /* the actual mouse button index */
    unsigned index;
    /* the expression to evaluate */
    Expression expression;
};

/* key binding */
struct configuration_key {
    /* the key modifiers */
    unsigned modifiers;
    /* additional flags */
    unsigned flags;
    /* the key symbol */
    KeySym key_symbol;
    /* the code of the key, used when `key_symbol` is NoSymbol */
    KeyCode key_code;
    /* the expression to evaluate */
    Expression expression;
};

/* association between class/instance and window number */
struct configuration_association {
    /* the window number */
    unsigned number;
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
    int overlap_percentage;
    /* the name of the cursor used on the root window */
    utf8_t *root_cursor;
    /* the name of the cursor used for moving a window */
    utf8_t *moving_cursor;
    /* the name of the cursor used for sizing a window horizontally */
    utf8_t *horizontal_cursor;
    /* the name of the cursor used for sizing a window vertically */
    utf8_t *vertical_cursor;
    /* the name of the cursor used for sizing a window */
    utf8_t *sizing_cursor;
};

/* assignment settings */
struct configuration_assignment {
    /* the number the first window gets assigned */
    int first_window_number;
    /* the associations that are wanted */
    struct configuration_association *associations;
    /* the number of associations */
    size_t number_of_associations;
};

/* tiling settings */
struct configuration_tiling {
    /* whether to automatically create a split when a window is shown */
    int auto_split;
    /* whether to automatically equalize all frames within the root */
    int auto_equalize;
    /* whether to fill in empty frames automatically */
    int auto_fill_void;
    /* whether to remove frames automatically when their inner windows is
     * hidden
     */
    int auto_remove;
    /* whether to remove frames automatically when they become empty */
    int auto_remove_void;
};

/* font settings */
struct configuration_font {
    /* font name in fontconfig format */
    utf8_t *name;
};

/* border settings */
struct configuration_border {
    /* width of the border around the windows */
    int size;
    /* color of the border of an unfocused window */
    int color;
    /* color of the border of an unfocused tiling window */
    int active_color;
    /* color of the border of a focused window */
    int focus_color;
};

/* gaps settings */
struct configuration_gaps {
    /* width of the inner gaps (between frames) */
    int inner[4];
    /* width of the outer gaps (between frames and monitor boundaries */
    int outer[4];
};

/* notification settings */
struct configuration_notification {
    /* the duration in seconds a notification window should linger for */
    int duration;
    /* padding of text within the notification window */
    int padding;
    /* width of the border */
    int border_size;
    /* color of the border around the window */
    int border_color;
    /* color of the text */
    int foreground;
    /* color of the background */
    int background;
};

/* mouse settings */
struct configuration_mouse {
    /* how many pixels off the edges of windows should be used for resizing */
    int resize_tolerance;
    /* the modifier key for all buttons (applied at the parsing step) */
    int modifiers;
    /* the modifiers to ignore for a mouse binding */
    int ignore_modifiers;
    /* the configured buttons */
    struct configuration_button *buttons;
    /* the number of configured buttons */
    size_t number_of_buttons;
};

/* keyboard settings */
struct configuration_keyboard {
    /* the modifier key for all keys (applied at the parsing step) */
    int modifiers;
    /* the modifiers to ignore for a key binding */
    int ignore_modifiers;
    /* the configured keys */
    struct configuration_key *keys;
    /* the number of configured keys */
    size_t number_of_keys;
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
