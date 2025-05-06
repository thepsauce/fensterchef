#include <X11/keysym.h>

#include "configuration/default.h"
#include "cursor.h"
#include "font.h"

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
    /* the singular action to execute */
    struct default_key_binding_action {
        /* the type of the action */
        action_type_t type;
        /* optional additional action data */
        const utf8_t *string;
    } action;
} default_key_bindings[] = {
    /* reload the configuration */
    { ShiftMask, XK_r, { .type = ACTION_RELOAD_CONFIGURATION } },

    /* move the focus to a child or parent frame */
    { 0, XK_a, { .type = ACTION_FOCUS_PARENT } },
    { 0, XK_b, { .type = ACTION_FOCUS_CHILD } },
    { ShiftMask, XK_a, { .type = ACTION_FOCUS_ROOT } },

    /* make the size of frames equal */
    { 0, XK_equal, { .type = ACTION_EQUALIZE } },

    /* close the active window */
    { 0, XK_q, { .type = ACTION_CLOSE_WINDOW } },

    /* minimize the active window */
    { 0, XK_minus, { .type = ACTION_MINIMIZE_WINDOW } },

    /* go to the next window in the tiling */
    { 0, XK_n, { .type = ACTION_SHOW_NEXT_WINDOW } },
    { 0, XK_p, { .type = ACTION_SHOW_PREVIOUS_WINDOW } },

    /* remove the current tiling frame */
    { 0, XK_r, { .type = ACTION_REMOVE } },

    /* put the stashed frame into the current one */
    { 0, XK_o, { .type = ACTION_POP_STASH } },

    /* toggle between tiling and the previous mode */
    { ShiftMask, XK_space, { .type = ACTION_TOGGLE_TILING } },

    /* toggle between fullscreen and the previous mode */
    { 0, XK_f, { .type = ACTION_TOGGLE_FULLSCREEN } },

    /* focus from tiling to non tiling and vise versa */
    { 0, XK_space, { .type = ACTION_TOGGLE_FOCUS } },

    /* split a frame */
    { 0, XK_v, { .type = ACTION_SPLIT_HORIZONTALLY } },
    { 0, XK_s, { .type = ACTION_SPLIT_VERTICALLY } },

    /* move between frames */
    { 0, XK_k, { .type = ACTION_FOCUS_UP } },
    { 0, XK_h, { .type = ACTION_FOCUS_LEFT } },
    { 0, XK_l, { .type = ACTION_FOCUS_RIGHT } },
    { 0, XK_j, { .type = ACTION_FOCUS_DOWN } },

    /* exchange frames */
    { ShiftMask, XK_k, { .type = ACTION_EXCHANGE_UP } },
    { ShiftMask, XK_h, { .type = ACTION_EXCHANGE_LEFT } },
    { ShiftMask, XK_l, { .type = ACTION_EXCHANGE_RIGHT } },
    { ShiftMask, XK_j, { .type = ACTION_EXCHANGE_DOWN } },

    /* show the interactive window list */
    { 0, XK_w, { .type = ACTION_SHOW_LIST } },

    /* run the terminal or xterm as fall back */
    { 0, XK_Return, { ACTION_RUN,
            "[ -n \"$TERMINAL\" ] && exec \"$TERMINAL\" || exec xterm"
        } },

    /* quit fensterchef */
    { ControlMask | ShiftMask, XK_e, { .type = ACTION_QUIT } }
};

/* default settings that can not be directly set in the settings below */
static const char *default_font = "Mono";
static const char *default_root_cursor = "left_ptr";
static const char *default_moving_cursor = "fleur";
static const char *default_horizontal_cursor = "sb_h_double_arrow";
static const char *default_vertical_cursor = "sb_v_double_arrow";
static const char *default_sizing_cursor = "sizing";

/* the settings of the default configuration */
const struct configuration default_configuration = {
    .modifiers = Mod4Mask,
    .modifiers_ignore = LockMask | Mod2Mask,

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
    .foreground = 0xff939388,
    .background = 0xff49494d,

    .gaps_inner = { 0, 0, 0, 0 },
    .gaps_outer = { 0, 0, 0, 0 },
};

/* Puts the button bindings of the default configuration into the current
 * configuration.
 */
static void merge_with_default_button_bindings(void)
{
    struct configuration_button *button;

    /* add the default buttons on top of the already defined buttons */
    REALLOCATE(configuration.buttons, configuration.number_of_buttons +
            SIZE(default_button_bindings));
    button = &configuration.buttons[configuration.number_of_buttons];
    for (unsigned i = 0; i < SIZE(default_button_bindings); i++) {
        /* bake a button binding from the default button bindings struct */
        button->is_transparent = false;
        button->is_release = default_button_bindings[i].is_release;
        button->modifiers = default_button_bindings[i].modifiers |
            configuration.modifiers;
        button->index = default_button_bindings[i].button_index;

        ALLOCATE(button->actions.items, 1);
        button->actions.items[0].type = default_button_bindings[i].type;
        button->actions.items[0].data_count = 0;
        button->actions.number_of_items = 1;
        button->actions.data = NULL;

        button++;
    }
    configuration.number_of_buttons += SIZE(default_button_bindings);
}

/* Puts the key bindings of the default configuration into the current
 * configuration.
 */
static void merge_with_default_key_bindings(void)
{
    struct configuration_key *key;

    /* add the new keys on top of the already defined keys */
    REALLOCATE(configuration.keys, configuration.number_of_keys +
            SIZE(default_key_bindings));
    key = &configuration.keys[configuration.number_of_keys];
    for (unsigned i = 0; i < SIZE(default_key_bindings); i++) {
        /* bake a key binding from the bindings array */
        key->is_release = false;
        key->modifiers = default_key_bindings[i].modifiers |
            configuration.modifiers;
        key->key_symbol = default_key_bindings[i].key_symbol;
        key->key_code = 0;

        ALLOCATE(key->actions.items, 1);
        key->actions.items[0].type = default_key_bindings[i].action.type;
        key->actions.number_of_items = 1;
        if (default_key_bindings[i].action.string != NULL) {
            key->actions.items[0].data_count = 1;
            ALLOCATE(key->actions.data, 1);
            key->actions.data->flags = PARSE_DATA_FLAGS_IS_POINTER;
            key->actions.data->u.string =
                xstrdup(default_key_bindings[i].action.string);
        } else {
            key->actions.items[0].data_count = 0;
        }

        key++;
    }
    configuration.number_of_keys += SIZE(default_key_bindings);
}

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags)
{
    if ((flags & DEFAULT_CONFIGURATION_MERGE_BUTTON_BINDINGS)) {
        merge_with_default_button_bindings();
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_KEY_BINDINGS)) {
        merge_with_default_key_bindings();
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_FONT)) {
        set_font(default_font);
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_CURSOR)) {
        configuration.root_cursor = load_cursor(default_root_cursor);
        configuration.moving_cursor = load_cursor(default_moving_cursor);
        configuration.horizontal_cursor =
            load_cursor(default_horizontal_cursor);
        configuration.vertical_cursor = load_cursor(default_vertical_cursor);
        configuration.sizing_cursor = load_cursor(default_sizing_cursor);
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_SETTINGS)) {
        copy_configuration_settings(&default_configuration);
    }
}
