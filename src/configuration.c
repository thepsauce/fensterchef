#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <X11/keysym.h>

#include "configuration_parser.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "render.h"
#include "screen.h"
#include "utility.h"
#include "window.h"

/* if the user requested to reload the configuration, handled in `main()` */
bool reload_requested;

/* the default configuration */
static const struct configuration default_configuration = {
    /* default general settings, currently not used for anything */
    .general = {
        .unused = 0
    },

    /* default tiling settings: fill empty frames but never automatically remove
     * them
     */
    .tiling = {
        .auto_remove_void = false,
        .auto_fill_void = true
    },

    /* default font settings: Mono */
    .font = {
        .name = (uint8_t*) "Mono"
    },

    /* default border settings: no borders */
    .border = {
        .size = 0,
    },

    /* default gap settings: no gaps */
    .gaps = {
        .inner = 0,
        .outer = 0
    },

    /* default notification settings */
    .notification = {
        .duration = 3,
        .padding = 6,
        .border_color = 0x000000,
        .border_size = 1,
        .foreground = 0x000000,
        .background = 0xffffff,
    },

    /* default key settings: Mod4 as main modifier, see
     * `merge_with_default_key_bindings()` for the default key bindings
     */
    .keyboard = {
        .modifiers = XCB_MOD_MASK_4,
        .ignore_modifiers = XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2 |
            XCB_MOD_MASK_3 | XCB_MOD_MASK_5,
    },
};

/* the currently loaded configuration */
struct configuration configuration;

/* Puts the keybindings of the default configuration into @configuration without
 * overwriting any keybindings.
 */
void merge_with_default_key_bindings(struct configuration *configuration)
{
    /* default keybindings, note that the modifiers are combined with
     * @configuration->keyboard.modifiers
     */
    struct {
        uint16_t modifiers;
        xcb_keysym_t key_symbol;
        Action action;
    } default_bindings[] = {
        { XCB_MOD_MASK_SHIFT, XK_r, { .code = ACTION_RELOAD_CONFIGURATION } },

        { 0, XK_n, { .code = ACTION_NEXT_WINDOW } },
        { 0, XK_p, { .code = ACTION_PREVIOUS_WINDOW } },

        { 0, XK_r, { .code = ACTION_REMOVE_FRAME } },

        { XCB_MOD_MASK_SHIFT, XK_space, { .code = ACTION_TOGGLE_TILING } },
        { 0, XK_space, { .code = ACTION_TRAVERSE_FOCUS } },

        { 0, XK_f, { .code = ACTION_TOGGLE_FULLSCREEN } },

        { 0, XK_v, { .code = ACTION_SPLIT_HORIZONTALLY } },
        { 0, XK_s, { .code = ACTION_SPLIT_VERTICALLY } },

        { 0, XK_k, { .code = ACTION_MOVE_UP } },
        { 0, XK_h, { .code = ACTION_MOVE_LEFT } },
        { 0, XK_l, { .code = ACTION_MOVE_RIGHT } },
        { 0, XK_j, { .code = ACTION_MOVE_DOWN } },

        { 0, XK_w, { .code = ACTION_SHOW_WINDOW_LIST } },

        { 0, XK_Return, { ACTION_RUN, {
            .string = (uint8_t*) "/usr/bin/xterm" } } },

        { XCB_MOD_MASK_SHIFT, XK_e, { .code = ACTION_QUIT } }
    };

    struct configuration_key *key;
    uint32_t new_count;
    struct configuration_key *next_key;

    /* get the number of keys not defined yet */
    new_count = configuration->keyboard.number_of_keys;
    for (uint32_t i = 0; i < SIZE(default_bindings); i++) {
        const uint16_t modifiers = default_bindings[i].modifiers |
            configuration->keyboard.modifiers;
        key = find_configured_key(configuration, modifiers,
                default_bindings[i].key_symbol);
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
    RESIZE(configuration->keyboard.keys, new_count);
    next_key = &configuration->keyboard.keys[configuration->keyboard.number_of_keys];
    for (uint32_t i = 0; i < SIZE(default_bindings); i++) {
        const uint16_t modifiers = default_bindings[i].modifiers |
            configuration->keyboard.modifiers;
        key = find_configured_key(configuration, modifiers,
                default_bindings[i].key_symbol);
        if (key != NULL) {
            continue;
        }
        next_key->modifiers = modifiers;
        next_key->key_symbol = default_bindings[i].key_symbol;
        next_key->actions = xmemdup(&default_bindings[i].action,
                sizeof(default_bindings[i].action));
        next_key->number_of_actions = 1;
        duplicate_data_value(get_action_data_type(next_key->actions[0].code),
                    &next_key->actions[0].parameter);
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

    /* add the default key bindings */
    merge_with_default_key_bindings(&configuration);

    set_configuration(&configuration);
}

/* Duplicate @key into itself. */
static void duplicate_key(struct configuration_key *key)
{
    key->actions = xmemdup(key->actions, sizeof(*key->actions) *
            key->number_of_actions);
    for (uint32_t i = 0; i < key->number_of_actions; i++) {
        duplicate_data_value(get_action_data_type(key->actions[i].code),
                &key->actions[i].parameter);
    }
}

/* Create a deep copy of @duplicate and put it into itself. */
void duplicate_configuration(struct configuration *duplicate)
{
    if (duplicate->font.name != NULL) {
        duplicate->font.name = (uint8_t*) xstrdup((char*) duplicate->font.name);
    }
    duplicate->keyboard.keys = xmemdup(duplicate->keyboard.keys,
            sizeof(*duplicate->keyboard.keys) *
            duplicate->keyboard.number_of_keys);
    for (uint32_t i = 0; i < duplicate->keyboard.number_of_keys; i++) {
        duplicate_key(&duplicate->keyboard.keys[i]);
    }
}

/* Clear the resources given configuration occupies. */
void clear_configuration(struct configuration *configuration)
{
    free(configuration->font.name);
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        free_key_actions(&configuration->keyboard.keys[i]);
    }
    free(configuration->keyboard.keys);
}

/* Load the user configuration and merge it into the current configuration. */
void reload_user_configuration(void)
{
    const char *home;
    char *path;
    struct configuration configuration;

    home = getenv("HOME");
    if (home != NULL) {
        path = xasprintf("%s/" FENSTERCHEF_CONFIGURATION, home);
        if (load_configuration_file(path, &configuration) == OK) {
            set_configuration(&configuration);
        }
        free(path);
    }
}

/* Helper function to free resources of a key struct. */
void free_key_actions(struct configuration_key *key)
{
    for (uint32_t i = 0; i < key->number_of_actions; i++) {
        clear_data_value(get_action_data_type(key->actions[i].code),
                &key->actions[i].parameter);
    }
    free(key->actions);
}

/* Get a key from key modifiers and a key symbol. */
struct configuration_key *find_configured_key(struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;

    /* find a matching key (the keysym AND modifiers must match up) */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->key_symbol == key_symbol && key->modifiers == modifiers) {
            return key;
        }
    }
    return NULL;
}

/* Grab the keybinds so we receive the keypress events for them. */
void grab_configured_keys(void)
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
        if (keycodes == NULL) {
            continue;
        }
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

                modifiers = (k | configuration.keyboard.keys[i].modifiers);

                xcb_grab_key(connection,
                        1, /* 1 means we specify a window for grabbing */
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

/* Reload the given frame or all sub frames. */
static void reload_frame_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        reload_frame_recursively(frame->left);
        reload_frame_recursively(frame->right);
    } else {
        reload_frame(frame);
    }
}

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *new_configuration)
{
    struct configuration old_configuration;
    xcb_render_color_t color;

    old_configuration = configuration;
    configuration = *new_configuration;

    /* check if font changed */
    if (configuration.font.name != NULL &&
            (old_configuration.font.name == NULL ||
                strcmp((char*) old_configuration.font.name,
                    (char*) configuration.font.name) != 0)) {
        set_font(configuration.font.name);
    }

    /* check if border size changed and update the border size of all windows */
    if (old_configuration.border.size != configuration.border.size) {
        for (Window *window = first_window; window != NULL;
                window = window->next) {
            if (!window->state.is_visible ||
                    !DOES_WINDOW_MODE_HAVE_BORDER(window->state.mode)) {
                continue;
            }
            general_values[0] = configuration.border.size;
            xcb_configure_window(connection, window->xcb_window,
                    XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
        }
    }

    /* check if border size or gaps change and reload all frames */
    if (old_configuration.border.size != configuration.border.size ||
            old_configuration.gaps.inner != configuration.gaps.inner ||
            old_configuration.gaps.outer != configuration.gaps.outer) {
        for (Monitor *monitor = screen->monitor; monitor != NULL;
                monitor = monitor->next) {
            reload_frame_recursively(monitor->frame);
        }
    }

    /* check if notification border color changed */
    if (old_configuration.notification.border_color !=
            configuration.notification.border_color) {
        general_values[0] = configuration.notification.border_color;
        xcb_change_window_attributes(connection, screen->notification_window,
                XCB_CW_BORDER_PIXEL, general_values);
        xcb_change_window_attributes(connection, screen->window_list_window,
                XCB_CW_BORDER_PIXEL, general_values);
    }
    /* check if notification border size changed */
    if (old_configuration.notification.border_size !=
            configuration.notification.border_size) {
        general_values[0] = configuration.notification.border_size;
        xcb_configure_window(connection, screen->notification_window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
        xcb_configure_window(connection, screen->window_list_window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, general_values);
    }
    /* check if notification background changed */
    if (old_configuration.notification.background !=
            configuration.notification.background) {
        convert_color_to_xcb_color(&color,
                configuration.notification.background);
        set_pen_color(stock_objects[STOCK_WHITE_PEN], color);
    }
    /* check if notification foreground changed */
    if (old_configuration.notification.foreground !=
            configuration.notification.foreground) {
        convert_color_to_xcb_color(&color,
                configuration.notification.foreground);
        set_pen_color(stock_objects[STOCK_BLACK_PEN], color);
    }
    /* check if notification foreground or background changed */
    if (old_configuration.notification.foreground !=
            configuration.notification.foreground ||
            old_configuration.notification.background !=
                configuration.notification.background) {
        general_values[0] = configuration.notification.background;
        general_values[1] = configuration.notification.foreground;
        xcb_change_gc(connection, stock_objects[STOCK_GC],
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, general_values);

        general_values[0] = configuration.notification.foreground;
        general_values[1] = configuration.notification.background;
        xcb_change_gc(connection, stock_objects[STOCK_INVERTED_GC],
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, general_values);
    }

    /* reset the alarm */
    alarm(configuration.notification.duration);

    /* just do this without checking if anything changed */
    grab_configured_keys();

    clear_configuration(&old_configuration);
}

/* Load the configuration within given file. */
int load_configuration_file(const char *file_name,
        struct configuration *destination_configuration)
{
    Parser parser;
    parser_error_t error;

    parser.file = fopen(file_name, "r");
    if (parser.file == NULL) {
        LOG_ERROR(NULL, "could not open %s: %s\n", file_name, strerror(errno));
        return ERROR;
    }

    parser.line_capacity = 128;
    parser.line = xmalloc(parser.line_capacity);
    parser.line_number = 0;

    parser.configuration = destination_configuration;
    *parser.configuration = configuration;
    /* disregard all previous keybinds */
    parser.configuration->keyboard.keys = NULL;
    parser.configuration->keyboard.number_of_keys = 0;
    duplicate_configuration(parser.configuration);

    parser.label = PARSER_LABEL_NONE;

    /* parse file line by line */
    while (read_next_line(&parser)) {
        error = parse_line(&parser);
        /* emit an error if a good line has any trailing characters */
        if (error == PARSER_SUCCESS && parser.line[parser.column] != '\0') {
            error = PARSER_ERROR_TRAILING;
        }

        if (error != PARSER_SUCCESS) {
            LOG("%s:%zu: %s\n", file_name, parser.line_number,
                    parser_string_error(error));
            LOG_ADDITIONAL("%5zu %s\n", parser.line_number, parser.line);
            for (int i = 0; i <= 5; i++) {
                LOG_ADDITIONAL(" ");
            }
            if (error == PARSER_ERROR_TRAILING) {
                /* indicate all trailing characters using "  ^~~~" */
                for (size_t i = 0; i < parser.column; i++) {
                    LOG_ADDITIONAL(" ");
                }
                LOG_ADDITIONAL("^");
                for (size_t i = parser.column + 1;
                        parser.line[i] != '\0'; i++) {
                    LOG_ADDITIONAL("~");
                }
                LOG_ADDITIONAL("\n");
            } else {
                /* indicate the error region using "  ~~~^" */
                for (size_t i = 0; i < parser.item_start_column; i++) {
                    LOG_ADDITIONAL(" ");
                }
                for (size_t i = parser.item_start_column + 1;
                        i < parser.column; i++) {
                    LOG_ADDITIONAL("~");
                }
                LOG_ADDITIONAL("^\n");
            }
        }

        if (error != PARSER_SUCCESS) {
            break;
        }
    }

    free(parser.line);
    fclose(parser.file);

    if (error != PARSER_SUCCESS) {
        clear_configuration(parser.configuration);
        LOG_ERROR(NULL, "got an error reading configuration file: \"%s\"",
                file_name);
        return ERROR;
    }

    LOG("successfully read configuration file: \"%s\"\n", file_name);

    return OK;
}
