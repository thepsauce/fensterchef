#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <X11/keysym.h>

#include "configuration_parser.h"
#include "fensterchef.h"
#include "render_font.h"
#include "screen.h"
#include "utility.h"

/* default font settings */
static const struct configuration_font default_font = {
    .name = (FcChar8*) "Noto Sans:size=16"
};

/* default border settings */
static const struct configuration_border default_border = {
    .size = 2,
    .color = 0x434343,
    .focus_color = 0xff0000
};

/* default gap settings */
static const struct configuration_gaps default_gaps = {
    .size = 2
};

/* default key binds */
static const struct configuration_key default_keys[] = {
    { XCB_MOD_MASK_SHIFT, XK_r, ACTION_RELOAD_CONFIGURATION },

    { 0, XK_Return, ACTION_START_TERMINAL },

    { 0, XK_n, ACTION_NEXT_WINDOW },
    { 0, XK_p, ACTION_PREV_WINDOW },

    { 0, XK_r, ACTION_REMOVE_FRAME },

    { XCB_MOD_MASK_SHIFT, XK_space, ACTION_CHANGE_WINDOW_STATE },
    { 0, XK_space, ACTION_TRAVERSE_FOCUS },

    { 0, XK_f, ACTION_TOGGLE_FULLSCREEN },

    { 0, XK_v, ACTION_SPLIT_HORIZONTALLY },
    { 0, XK_s, ACTION_SPLIT_VERTICALLY },

    { 0, XK_k, ACTION_MOVE_UP },
    { 0, XK_h, ACTION_MOVE_LEFT },
    { 0, XK_l, ACTION_MOVE_RIGHT },
    { 0, XK_j, ACTION_MOVE_DOWN },

    { 0, XK_w, ACTION_SHOW_WINDOW_LIST },

    { XCB_MOD_MASK_SHIFT, XK_e, ACTION_QUIT_WM },
};

/* default key settings */
static const struct configuration_keyboard default_keyboard = {
    .main_modifiers = XCB_MOD_MASK_4,
    .ignore_modifiers = XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2 |
        XCB_MOD_MASK_3 | XCB_MOD_MASK_5,
    .keys = (struct configuration_key*) default_keys,
    .number_of_keys = SIZE(default_keys),
};

/* the currently loaded configuration */
struct configuration configuration;

/* Load the default values into the configuration. */
void load_default_configuration(void)
{
    memcpy(&configuration.font, &default_font, sizeof(default_font));
    configuration.font.name = xstrdup((char*) configuration.font.name);

    memcpy(&configuration.border, &default_border, sizeof(default_border));
    memcpy(&configuration.gaps, &default_gaps, sizeof(default_gaps));

    memcpy(&configuration.keyboard, &default_keyboard, sizeof(default_keyboard));

    configuration.keyboard.keys = xmemdup(
            configuration.keyboard.keys,
            sizeof(*configuration.keyboard.keys) * 
            configuration.keyboard.number_of_keys);

    init_configured_keys();
    set_font(configuration.font.name);
}

/* Load the user configuration and merge it into the current configuration. */
void reload_user_configuration(void)
{
    const char *home;
    char *path;
    struct configuration configuration;
    merge_t merge;

    home = getenv("HOME");
    if (home != NULL) {
        path = xasprintf("%s/" FENSTERCHEF_CONFIGURATION, home);
        if (load_configuration_file(path, &configuration, &merge) == 0) {
            merge_configuration(&configuration, merge);
        }
        free(path);
    }
}

/* Get a key from key modifiers and a key symbol. */
static struct configuration_key *find_configured_key(struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol)
{
    struct configuration_key *key;

    if (!(modifiers & configuration->keyboard.main_modifiers)) {
        return NULL;
    }

    modifiers ^= configuration->keyboard.main_modifiers;
    modifiers &= ~configuration->keyboard.ignore_modifiers;

    /* find a matching key (the keysym AND modifier must match up) */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->key_symbol == key_symbol && key->modifiers == modifiers) {
            return key;
        }
    }
    return NULL;
}

/* Get an action value from key modifiers and a key symbol. */
action_t find_configured_action(uint16_t modifiers, xcb_keysym_t key_symbol)
{
    struct configuration_key *key;

    key = find_configured_key(&configuration, modifiers, key_symbol);
    return key == NULL ? ACTION_NULL : key->action;
}

/* Grab the keybinds so we receive the keypress events for them. */
void init_configured_keys(void)
{
    xcb_window_t root;
    xcb_keycode_t *keycodes;
    uint16_t modifiers;

    root = screen->xcb_screen->root;

    /* remove all previously grabbed keys so that we can overwrite them */
    xcb_ungrab_key(connection, XCB_GRAB_ANY, root, XCB_MOD_MASK_ANY);

    for (uint32_t i = 0; i < configuration.keyboard.number_of_keys; i++) {
        /* go over all keycodes of a specific key symbol and grab them with
         * needed modifiers
         */
        keycodes = get_keycodes(configuration.keyboard.keys[i].key_symbol);
        for (uint32_t j = 0; keycodes[j] != XCB_NO_SYMBOL; j++) {
            /* use every possible combination of modifiers we do not care about
             * so that when the user has CAPS LOCK for example, it does not mess
             * with keybinds.
             */
            for (uint32_t k = 0; k < (uint32_t) (1 << 8); k++) {
                /* check if @k has any outside modifiers */
                if ((k | configuration.keyboard.ignore_modifiers) !=
                        configuration.keyboard.ignore_modifiers) {
                    continue;
                }

                modifiers = configuration.keyboard.main_modifiers |
                    configuration.keyboard.keys[i].modifiers;

                xcb_grab_key(connection,
                        1, /* 1 means we specify a specific window for grabbing */
                        root, /* this is the window we grab the key for */
                        modifiers, keycodes[j],
                        /* the ASYNC constants are so that event processing
                         * continues normally, otherwise we would need to issue
                         * an AllowEvents request each time we receive KeyPress
                         * events
                         */
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC);
            }
        }
        free(keycodes);
    }
}

/* Merges given configuration into the current configuration. */
void merge_configuration(struct configuration *new_configuration, merge_t merge)
{
    uint32_t remaining_count;
    struct configuration_key *key;

    /* merge font */
    if ((merge & MERGE_FONT_NAME)) {
        if (strcmp((char*) configuration.font.name,
                    (char*) new_configuration->font.name) != 0) {
            free(configuration.font.name);
            configuration.font.name = new_configuration->font.name;
            set_font(configuration.font.name);
        } else {
            free(new_configuration->font.name);
        }
    }

    /* merge border */
    if ((merge & MERGE_BORDER_SIZE)) {
        configuration.border.size = new_configuration->border.size;
    }
    if ((merge & MERGE_BORDER_COLOR)) {
        configuration.border.color = new_configuration->border.color;
    }
    if ((merge & MERGE_BORDER_FOCUS_COLOR)) {
        configuration.border.focus_color = new_configuration->border.focus_color;
    }

    /* merge gaps */
    if ((merge & MERGE_GAPS_SIZE)) {
        configuration.gaps.size = new_configuration->gaps.size;
    }

    /* merge keyboard */
    if ((merge & MERGE_KEYBOARD_MAIN_MODIFIERS)) {
        configuration.keyboard.main_modifiers =
            new_configuration->keyboard.main_modifiers;
    }

    if ((merge & MERGE_KEYBOARD_IGNORE_MODIFIERS)) {
        configuration.keyboard.ignore_modifiers =
            new_configuration->keyboard.ignore_modifiers;
    }

    if ((merge & MERGE_KEYBOARD_KEYS)) {
        remaining_count = new_configuration->keyboard.number_of_keys;
        for (uint32_t i = 0; i < new_configuration->keyboard.number_of_keys; i++) {
            key = find_configured_key(&configuration,
                    new_configuration->keyboard.keys[i].modifiers,
                    new_configuration->keyboard.keys[i].key_symbol);
            if (key != NULL) {
                key->action = new_configuration->keyboard.keys[i].action;
                new_configuration->keyboard.keys[i].key_symbol = (xcb_keysym_t) -1;
                remaining_count--;
            }
        }

        RESIZE(configuration.keyboard.keys, configuration.keyboard.number_of_keys +
                remaining_count);
        for (uint32_t i = 0; i < new_configuration->keyboard.number_of_keys; i++) {
            if (new_configuration->keyboard.keys[i].key_symbol == (xcb_keysym_t) -1) {
                continue;
            }
            configuration.keyboard.keys[configuration.keyboard.number_of_keys++] =
                new_configuration->keyboard.keys[i];
        }

        free(new_configuration->keyboard.keys);
    }

    if ((merge & (MERGE_KEYBOARD_MAIN_MODIFIERS |
                    MERGE_KEYBOARD_IGNORE_MODIFIERS |
                    MERGE_KEYBOARD_KEYS))) {
        init_configured_keys();
    }
}

/* Load the configuration within given file. */
int load_configuration_file(const char *file_name,
        struct configuration *configuration, merge_t *merge)
{
    struct parser_context context;
    int error;

    context.file = fopen(file_name, "r");
    if (context.file == NULL) {
        return 1;
    }

    context.line_capacity = 128;
    context.line = xmalloc(context.line_capacity);

    memset(configuration, 0, sizeof(*configuration));
    context.configuration = configuration;
    
    context.merge = MERGE_NONE;

    context.label = PARSER_LABEL_NONE;

    error = 0;
    while (read_next_line(&context)) {
        if (parse_line(&context)) {
            error = 1;
            break;
        }
    }

    free(context.line);
    fclose(context.file);

    if (error != 0) {
        free(configuration->font.name);
        free(configuration->keyboard.keys);
    }

    *merge = context.merge;

    return error;
}
