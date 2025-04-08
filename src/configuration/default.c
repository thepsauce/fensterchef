#include <X11/keysym.h>

#include <string.h>

#include "configuration/default.h"
#include "configuration/expression.h"
#include "utility.h"

/* the default configuration */
const struct configuration default_configuration = {
    .general = {
        .overlap_percentage = 80,
        .root_cursor = "left_ptr",
        .moving_cursor = "fleur",
        .horizontal_cursor = "sb_h_double_arrow",
        .vertical_cursor = "sb_v_double_arrow",
        .sizing_cursor = "sizing",
    },

    .assignment = {
        .first_window_number = 1,
    },

    .tiling = {
        .auto_split = false,
        .auto_equalize = true,
        .auto_fill_void = true,
        .auto_remove = false,
        .auto_remove_void = false,
    },

    .font = {
        .name = "Mono",
    },

    .border = {
        .size = 1,
        .color = 0xff36454f,
        .active_color = 0xff71797e,
        .focus_color = 0xffc7bb28,
    },

    .gaps = {
        .inner = { 0, 0, 0, 0 },
        .outer = { 0, 0, 0, 0 },
    },

    .notification = {
        .duration = 2,
        .padding = 6,
        .border_size = 1,
        .border_color = 0xff000000,
        .foreground = 0xff000000,
        .background = 0xffffffff,
    },

    .mouse = {
        .resize_tolerance = 8,
        .modifiers = Mod4Mask,
        .ignore_modifiers = LockMask | Mod2Mask,
    },

    .keyboard = {
        .modifiers = Mod4Mask,
        .ignore_modifiers = LockMask | Mod2Mask,
    },
};

/* Puts the mousebindings of the default configuration into @configuration
 * without overwriting any mousebindings.
 */
void merge_with_default_button_bindings(struct configuration *configuration)
{
    /* default mousebindings, note that the modifiers are combined with
     * @configuration->mouse.modifiers
     */
    struct {
        /* the modifiers of the button */
        unsigned modifiers;
        /* the binding flags */
        unsigned flags;
        /* the button to press */
        int button_index;
        /* the singular action to execute */
        action_type_t type;
    } default_bindings[] = {
        /* start moving or resizing a window (depends on the mouse position) */
        { 0, 0, 1, ACTION_INITIATE_RESIZE },
        /* minimize (hide) a window */
        { 0, BINDING_FLAG_RELEASE, 2, ACTION_MINIMIZE_WINDOW },
        /* start moving a window */
        { 0, 0, 3, ACTION_INITIATE_MOVE },
    };

    struct configuration_button *button;
    uint32_t new_count;
    struct configuration_button *next_button;

    /* get the number of buttons not defined yet */
    new_count = configuration->mouse.number_of_buttons;
    for (unsigned i = 0; i < SIZE(default_bindings); i++) {
        const unsigned modifiers = default_bindings[i].modifiers |
            configuration->mouse.modifiers;
        button = find_configured_button(configuration, modifiers,
                default_bindings[i].button_index, default_bindings[i].flags);
        if (button != NULL) {
            continue;
        }
        new_count++;
    }

    /* shortcut: no buttons need to be added */
    if (new_count == configuration->mouse.number_of_buttons) {
        return;
    }

    /* add the new buttons on top of the already defined buttons */
    REALLOCATE(configuration->mouse.buttons, new_count);
    next_button =
        &configuration->mouse.buttons[configuration->mouse.number_of_buttons];
    for (unsigned i = 0; i < SIZE(default_bindings); i++) {
        const unsigned modifiers = default_bindings[i].modifiers |
            configuration->mouse.modifiers;
        button = find_configured_button(configuration, modifiers,
                default_bindings[i].button_index, default_bindings[i].flags);
        if (button != NULL) {
            continue;
        }
        /* bake a button binding from the bindings array */
        next_button->flags = default_bindings[i].flags;
        next_button->modifiers = modifiers;
        next_button->index = default_bindings[i].button_index;
        initialize_expression_from_action(&next_button->expression,
                default_bindings[i].type, NULL);
        next_button++;
    }
    configuration->mouse.number_of_buttons = new_count;
}

/* Puts the keybindings of the default configuration into @configuration without
 * overwriting any keybindings.
 */
void merge_with_default_key_bindings(struct configuration *configuration)
{
    /* default keybindings, note that the modifiers are combined with
     * @configuration->keyboard.modifiers
     */
    struct {
        /* the modifiers of the key */
        unsigned modifiers;
        /* the key symbol */
        KeySym key_symbol;
        /* the singular action to execute */
        struct {
            /* the type of the action */
            action_type_t type;
            /* if the action has data */
            bool has_data;
            /* the action data */
            GenericData data;
        } action;
    } default_bindings[] = {
        /* reload the configuration */
        { ShiftMask, XK_r, { .type = ACTION_RELOAD_CONFIGURATION } },

        /* move the focus to a child or parent frame */
        { 0, XK_a, { ACTION_FOCUS_PARENT, true, { .integer = 1 } } },
        { 0, XK_b, { ACTION_FOCUS_CHILD, true, { .integer = 1 } } },
        { ShiftMask, XK_a, { ACTION_FOCUS_PARENT, true, {
                                       .integer = UINT32_MAX } } },

        /* make the size of frames equal */
        { 0, XK_equal, { .type = ACTION_EQUALIZE_FRAME } },

        /* close the active window */
        { 0, XK_q, { .type = ACTION_CLOSE_WINDOW } },

        /* minimize the active window */
        { 0, XK_minus, { .type = ACTION_MINIMIZE_WINDOW } },

        /* go to the next window in the tiling */
        { 0, XK_n, { .type = ACTION_NEXT_WINDOW } },
        { 0, XK_p, { .type = ACTION_PREVIOUS_WINDOW } },

        /* remove the current tiling frame */
        { 0, XK_r, { .type = ACTION_REMOVE_FRAME } },

        /* put the stashed frame into the current one */
        { 0, XK_o, { .type = ACTION_OTHER_FRAME } },

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

        /* move a window */
        { 0, XK_Left, { ACTION_RESIZE_BY, true, {
                .quad = { 20, 0, -20, 0 } } } },
        { 0, XK_Up, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 20, 0, -20 } } } },
        { 0, XK_Right, { ACTION_RESIZE_BY, true, {
                .quad = { -20, 0, 20, 0 } } } },
        { 0, XK_Down, { ACTION_RESIZE_BY, true, {
                .quad = { 0, -20, 0, 20 } } } },

        /* resizing the top/left edges of a window */
        { ControlMask, XK_Left, { ACTION_RESIZE_BY, true, {
                .quad = { 20, 0, 0, 0 } } } },
        { ControlMask, XK_Up, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 20, 0, 0 } } } },
        { ControlMask, XK_Right, { ACTION_RESIZE_BY, true, {
                .quad = { -20, 0, 0, 0 } } } },
        { ControlMask, XK_Down, { ACTION_RESIZE_BY, true, {
                .quad = { 0, -20, 0, 0 } } } },

        /* resizing the bottom/right edges of a window */
        { ShiftMask, XK_Left, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 0, -20, 0 } } } },
        { ShiftMask, XK_Up, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 0, 0, -20 } } } },
        { ShiftMask, XK_Right, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 0, 20, 0 } } } },
        { ShiftMask, XK_Down, { ACTION_RESIZE_BY, true, {
                .quad = { 0, 0, 0, 20 } } } },

        /* inflate/deflate a window */
        { ControlMask, XK_equal, {
                ACTION_RESIZE_BY, true, { .quad = { 10, 10, 10, 10 } } } },
        { ControlMask, XK_minus, {
                ACTION_RESIZE_BY, true, { .quad = { -10, -10, -10, -10 } } } },

        /* show the interactive window list */
        { 0, XK_w, { .type = ACTION_SHOW_WINDOW_LIST } },

        /* run the terminal or xterm as fall back */
        { 0, XK_Return, { ACTION_RUN, true, {
                .string = (utf8_t*)
                    "[ -n \"$TERMINAL\" ] && exec \"$TERMINAL\" || exec xterm"
            } } },

        /* quit fensterchef */
        { ControlMask | ShiftMask, XK_e, { .type = ACTION_QUIT } }
    };

    struct configuration_key *key;
    uint32_t new_count;
    struct configuration_key *next_key;

    /* get the number of keys not defined yet */
    new_count = configuration->keyboard.number_of_keys;
    for (unsigned i = 0; i < SIZE(default_bindings); i++) {
        const unsigned modifiers = default_bindings[i].modifiers |
            configuration->keyboard.modifiers;
        key = find_configured_key_by_key_symbol(configuration, modifiers,
                default_bindings[i].key_symbol, 0);
        if (key != NULL) {
            continue;
        }
        new_count++;
    }

    /* shortcut: no keys need to be added */
    if (new_count == configuration->keyboard.number_of_keys) {
        return;
    }

    /* add the new keys on top of the already defined keys */
    REALLOCATE(configuration->keyboard.keys, new_count);
    next_key =
        &configuration->keyboard.keys[configuration->keyboard.number_of_keys];
    for (unsigned i = 0; i < SIZE(default_bindings); i++) {
        const unsigned modifiers = default_bindings[i].modifiers |
            configuration->keyboard.modifiers;
        key = find_configured_key_by_key_symbol(configuration, modifiers,
                default_bindings[i].key_symbol, 0);
        if (key != NULL) {
            continue;
        }
        /* bake a key binding from the bindings array */
        next_key->flags = 0;
        next_key->modifiers = modifiers;
        next_key->key_symbol = default_bindings[i].key_symbol;
        next_key->key_code = 0;
        initialize_expression_from_action(&next_key->expression,
                default_bindings[i].action.type,
                !default_bindings[i].action.has_data ? NULL :
                    &default_bindings[i].action.data);
        next_key++;
    }
    configuration->keyboard.number_of_keys = new_count;
}

/* Load the default values into the configuration. */
void load_default_configuration(void)
{
    struct configuration configuration;

    /* create a duplicate of the default configuration */
    configuration = default_configuration;
    duplicate_configuration(&configuration);

    /* add the default bindings */
    merge_with_default_button_bindings(&configuration);
    merge_with_default_key_bindings(&configuration);

    set_configuration(&configuration);
}
