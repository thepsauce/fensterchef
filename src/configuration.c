#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "configuration_parser.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"

/* the currently loaded configuration */
struct configuration configuration;

/* Copy the button bindings of @duplicate into itself. */
static void duplicate_configuration_button_bindings(
        struct configuration *duplicate)
{
    /* duplicate mouse button bindings */
    duplicate->mouse.buttons = xmemdup(duplicate->mouse.buttons,
            sizeof(*duplicate->mouse.buttons) *
            duplicate->mouse.number_of_buttons);
    for (uint32_t i = 0; i < duplicate->mouse.number_of_buttons; i++) {
        struct configuration_button *const button =
            &duplicate->mouse.buttons[i];
        button->actions = duplicate_actions(button->actions,
                button->number_of_actions);
    }
}

/* Copy the key bindings of @duplicate into itself. */
static void duplicate_configuration_key_bindings(
        struct configuration *duplicate)
{
    /* duplicate key bindings */
    duplicate->keyboard.keys = xmemdup(duplicate->keyboard.keys,
            sizeof(*duplicate->keyboard.keys) *
            duplicate->keyboard.number_of_keys);
    for (uint32_t i = 0; i < duplicate->keyboard.number_of_keys; i++) {
        struct configuration_key *const key = &duplicate->keyboard.keys[i];
        key->actions = duplicate_actions(key->actions, key->number_of_actions);
    }
}

/* Create a deep copy of @duplicate and put it into itself. */
void duplicate_configuration(struct configuration *duplicate)
{
    if (duplicate->font.name != NULL) {
        duplicate->font.name = (uint8_t*) xstrdup((char*) duplicate->font.name);
    }
    duplicate_configuration_button_bindings(duplicate);
    duplicate_configuration_key_bindings(duplicate);
}

/* Clear the resources given configuration occupies. */
void clear_configuration(struct configuration *configuration)
{
    free(configuration->font.name);
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        free_actions(configuration->keyboard.keys[i].actions,
                configuration->keyboard.keys[i].number_of_actions);
    }
    free(configuration->keyboard.keys);
}

/* Load the user configuration and merge it into the current configuration. */
void reload_user_configuration(void)
{
    char *path;
    struct configuration configuration;

    if (fensterchef_configuration[0] == '~' &&
            fensterchef_configuration[1] == '/') {
        const char *const home = getenv("HOME");
        if (home == NULL) {
            LOG_ERROR("could not get home directory ($HOME is unset)\n");
            return;
        }
        path = xasprintf("%s/%s", home, &fensterchef_configuration[2]);
    } else {
        path = xstrdup(fensterchef_configuration);
    }

    if (load_configuration_file(path, &configuration) == OK) {
        set_configuration(&configuration);
    }

    free(path);
}

/* Get a key from button modifiers and a button index. */
struct configuration_button *find_configured_button(
        struct configuration *configuration,
        uint16_t modifiers, xcb_button_t button_index, uint16_t flags)
{
    struct configuration_button *button;

    /* remove the ignored modifiers but also ~0xff which is all the mouse button
     * masks
     */
    modifiers &= ~(configuration->mouse.ignore_modifiers | ~0xff);
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching button (the button AND modifiers must match up) */
    for (uint32_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        button = &configuration->mouse.buttons[i];
        if (button->index == button_index &&
                button->modifiers == modifiers &&
                (button->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            return button;
        }
    }
    return NULL;
}

/* Grab the mousebindings so we receive MousePress/MouseRelease events for
 * them.
 */
void grab_configured_buttons(void)
{
    xcb_window_t root;
    struct configuration_button *button;

    root = screen->root;

    /* remove all previously grabbed buttons so that we can overwrite them */
    xcb_ungrab_button(connection, XCB_GRAB_ANY, root, XCB_MOD_MASK_ANY);

    for (uint32_t i = 0; i < configuration.mouse.number_of_buttons; i++) {
        button = &configuration.mouse.buttons[i];
        /* use every possible combination of modifiers we do not care about
         * so that when the user has CAPS LOCK for example, it does not mess
         * with mousebindings
         */
        for (uint32_t j = 0; j < (uint32_t) (1 << 8); j++) {
            /* check if @j has any outside modifiers */
            if ((j | configuration.mouse.ignore_modifiers) !=
                    configuration.mouse.ignore_modifiers) {
                continue;
            }

            xcb_grab_button(connection,
                    1, /* 1 means we specify a window for grabbing */
                    root, /* this is the window we grab the button for */
                    (button->flags & BINDING_FLAG_RELEASE) ?
                    XCB_EVENT_MASK_BUTTON_RELEASE : XCB_EVENT_MASK_BUTTON_PRESS,
                    /* SYNC means that pointer (mouse) events will be frozen
                     * until we issue a AllowEvents request
                     */
                    XCB_GRAB_MODE_SYNC,
                    /* do not freeze keyboard events */
                    XCB_GRAB_MODE_ASYNC,
                    XCB_NONE, /* no confinement of the pointer */
                    XCB_NONE, /* no change of cursor */
                    button->index, (j | button->modifiers));
        }
    }
}

/* Get a key from key modifiers and a key symbol. */
struct configuration_key *find_configured_key(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol, uint16_t flags)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching key (the keysym AND modifiers must match up) */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->key_symbol == key_symbol && key->modifiers == modifiers &&
                (key->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            return key;
        }
    }
    return NULL;
}

/* Grab the keybindings so we receive the KeyPress/KeyRelease events for them.
 */
void grab_configured_keys(void)
{
    xcb_window_t root;
    xcb_keycode_t *keycodes;
    uint16_t modifiers;

    root = screen->root;

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
             * with keybindings.
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
                        /* do not freeze pointer (mouse) events */
                        XCB_GRAB_MODE_ASYNC,
                        /* SYNC means that keyboard events will be frozen until
                         * we issue a AllowEvents request
                         */
                        XCB_GRAB_MODE_SYNC);
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

    /* check if border size or gaps change and reload all frames */
    if (old_configuration.border.size != configuration.border.size ||
            old_configuration.gaps.inner != configuration.gaps.inner ||
            old_configuration.gaps.outer != configuration.gaps.outer) {
        for (Monitor *monitor = first_monitor; monitor != NULL;
                monitor = monitor->next) {
            reload_frame_recursively(monitor->frame);
        }
    }

    /* refresh the border size and color of all windows */
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (window == focus_window) {
            window->border_color = configuration.border.focus_color;
        } else {
            window->border_color = configuration.border.color;
        }
        window->border_size = configuration.border.size;
    }

    /* change border color and size of the notification window */
    change_client_attributes(&notification,
            configuration.notification.border_color);
    configure_client(&notification, notification.x, notification.y,
            notification.width, notification.height,
            configuration.notification.border_size);

    /* change border color and size of the window list window */
    change_client_attributes(&window_list.client,
            configuration.notification.border_color);
    configure_client(&window_list.client, window_list.client.x,
            window_list.client.y, window_list.client.width,
            window_list.client.height, configuration.notification.border_size);

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

    /* re-grab all bindings */
    grab_configured_buttons();
    grab_configured_keys();

    /* free the resources of the old configuration */
    clear_configuration(&old_configuration);
}

/* Load the configuration within given file. */
int load_configuration_file(const char *file_name,
        struct configuration *destination_configuration)
{
    Parser parser;
    parser_error_t error;

    memset(&parser, 0, sizeof(parser));

    parser.file = fopen(file_name, "r");
    if (parser.file == NULL) {
        LOG_ERROR("could not open configuration file %s: %s\n",
                file_name, strerror(errno));
        return ERROR;
    }

    parser.line_capacity = 128;
    parser.line = xmalloc(parser.line_capacity);

    parser.configuration = destination_configuration;
    *parser.configuration = configuration;
    /* disregard all previous bindings */
    parser.configuration->mouse.buttons = NULL;
    parser.configuration->mouse.number_of_buttons = 0;
    parser.configuration->keyboard.keys = NULL;
    parser.configuration->keyboard.number_of_keys = 0;
    duplicate_configuration(parser.configuration);

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
            fprintf(stderr, "%5zu %s\n", parser.line_number, parser.line);
            for (int i = 0; i <= 5; i++) {
                fprintf(stderr, " ");
            }
            if (error == PARSER_ERROR_TRAILING) {
                /* indicate all trailing characters using "  ^~~~" */
                for (size_t i = 0; i < parser.column; i++) {
                    fprintf(stderr, " ");
                }
                fprintf(stderr, "^");
                for (size_t i = parser.column + 1;
                        parser.line[i] != '\0'; i++) {
                    fprintf(stderr, "~");
                }
                fprintf(stderr, "\n");
            } else {
                /* indicate the error region using "  ~~~^" */
                for (size_t i = 0; i < parser.item_start_column; i++) {
                    fprintf(stderr, " ");
                }
                for (size_t i = parser.item_start_column + 1;
                        i < parser.column; i++) {
                    fprintf(stderr, "~");
                }
                fprintf(stderr, "^\n");
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
        LOG("got an error reading configuration file: %s\n", file_name);
        return ERROR;
    }

    /* set the existing button bindings if no mouse section is specified */
    if (!parser.has_label[PARSER_LABEL_MOUSE]) {
        parser.configuration->mouse.buttons =
            configuration.mouse.buttons;
        parser.configuration->mouse.number_of_buttons =
            configuration.mouse.number_of_buttons;
        duplicate_configuration_button_bindings(parser.configuration);
    }

    /* set the existing key bindings if no keyboard section is specified */
    if (!parser.has_label[PARSER_LABEL_KEYBOARD]) {
        parser.configuration->keyboard.keys =
            configuration.keyboard.keys;
        parser.configuration->keyboard.number_of_keys =
            configuration.keyboard.number_of_keys;
        duplicate_configuration_key_bindings(parser.configuration);
    }

    LOG("successfully read configuration file: %s\n", file_name);

    return OK;
}
