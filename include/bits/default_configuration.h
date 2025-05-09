#ifndef BITS__DEFAULT_CONFIGURATION_H
#define BITS__DEFAULT_CONFIGURATION_H

/**
 * Definition of the the default configuration.
 *
 * This is meant to be included ONCE.  The linker will complain if this rule is
 * broken.
 *
 * Use the functions in `include/configuration.h` to set default options.
 */

#include <X11/X.h>
#include <X11/keysym.h>

#include <stdbool.h>

#include "action.h"
#include "configuration.h"

/* default mouse bindings */
static const struct default_button_binding {
    /* the binding flags */
    bool is_release;
    /* the modifiers of the button */
    unsigned modifiers;
    /* the button to press */
    int button_index;
    /* the singular action to execute */
    action_type_t type;
} default_button_bindings[] = {
    /* start moving or resizing a window (depends on the mouse position) */
    { false, 0, 1, ACTION_INITIATE_RESIZE },
    /* minimize (hide) a window */
    { true, 0, 2, ACTION_MINIMIZE_WINDOW },
    /* start moving a window */
    { false, 0, 3, ACTION_INITIATE_MOVE },
};

/* default key bindings */
static const struct default_key_binding {
    /* the modifiers of the key */
    unsigned modifiers;
    /* the key symbol */
    KeySym key_symbol;
    /* the type of the action */
    action_type_t action;
    /* optional additional action data */
    const utf8_t *string;
} default_key_bindings[] = {
    /* reload the configuration */
    { ShiftMask, XK_r, .action = ACTION_RELOAD_CONFIGURATION },

    /* move the focus to a child or parent frame */
    { 0, XK_a, .action = ACTION_FOCUS_PARENT },
    { 0, XK_b, .action = ACTION_FOCUS_CHILD },
    { ShiftMask, XK_a, .action = ACTION_FOCUS_ROOT },

    /* make the size of frames equal */
    { 0, XK_equal, .action = ACTION_EQUALIZE },

    /* close the active window */
    { 0, XK_q, .action = ACTION_CLOSE_WINDOW },

    /* minimize the active window */
    { 0, XK_minus, .action = ACTION_MINIMIZE_WINDOW },

    /* go to the next window in the tiling */
    { 0, XK_n, .action = ACTION_SHOW_NEXT_WINDOW },
    { 0, XK_p, .action = ACTION_SHOW_PREVIOUS_WINDOW },

    /* remove the current tiling frame */
    { 0, XK_r, .action = ACTION_REMOVE },

    /* put the stashed frame into the current one */
    { 0, XK_o, .action = ACTION_POP_STASH },

    /* toggle between tiling and the previous mode */
    { ShiftMask, XK_space, .action = ACTION_TOGGLE_TILING },

    /* toggle between fullscreen and the previous mode */
    { 0, XK_f, .action = ACTION_TOGGLE_FULLSCREEN },

    /* focus from tiling to non tiling and vise versa */
    { 0, XK_space, .action = ACTION_TOGGLE_FOCUS },

    /* split a frame */
    { 0, XK_v, .action = ACTION_SPLIT_HORIZONTALLY },
    { 0, XK_s, .action = ACTION_SPLIT_VERTICALLY },

    /* move between frames */
    { 0, XK_k, .action = ACTION_FOCUS_UP },
    { 0, XK_h, .action = ACTION_FOCUS_LEFT },
    { 0, XK_l, .action = ACTION_FOCUS_RIGHT },
    { 0, XK_j, .action = ACTION_FOCUS_DOWN },

    /* exchange frames */
    { ShiftMask, XK_k, .action = ACTION_EXCHANGE_UP },
    { ShiftMask, XK_h, .action = ACTION_EXCHANGE_LEFT },
    { ShiftMask, XK_l, .action = ACTION_EXCHANGE_RIGHT },
    { ShiftMask, XK_j, .action = ACTION_EXCHANGE_DOWN },

    /* show the interactive window list */
    { 0, XK_w, .action = ACTION_SHOW_LIST },

    /* run the terminal or xterm as fall back */
    { 0, XK_Return, ACTION_RUN,
            "[ -n \"$TERMINAL\" ] && exec \"$TERMINAL\" || exec xterm"
    },

    /* quit fensterchef */
    { ControlMask | ShiftMask, XK_e, .action = ACTION_QUIT }
};

/* default settings that can not be directly set in the settings below */
static const char *default_font = "Mono";

/* the settings of the default configuration */
const struct configuration default_configuration = {
    .resize_tolerance = 8,

    .first_window_number = 1,

    .overlap = 80,

    .auto_split = false,
    .auto_equalize = true,
    .auto_fill_void = true,
    .auto_remove = false,
    .auto_remove_void = false,

    .notification_duration = 2,

    .text_padding = 6,

    .border_size = 1,
    .border_color = 0xff49494d,
    .border_color_active = 0xff939388,
    .border_color_focus = 0xff7fd0f1,
    .foreground = 0xff7fd0f1,
    .background = 0xff49494d,

    .gaps_inner = { 0, 0, 0, 0 },
    .gaps_outer = { 0, 0, 0, 0 },
};

#endif
