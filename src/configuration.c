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
#include "resources.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"

/* the currently loaded configuration */
struct configuration configuration;

/* Copy the associations of @duplicate into itself. */
static void duplicate_configuration_associations(
        struct configuration *duplicate)
{
    duplicate->assignment.associations = xmemdup(
            duplicate->assignment.associations,
            sizeof(*duplicate->assignment.associations) *
                duplicate->assignment.number_of_associations);
    for (uint32_t i = 0;
            i < duplicate->assignment.number_of_associations;
            i++) {
        struct configuration_association *const association =
            &duplicate->assignment.associations[i];
        association->instance_pattern = (utf8_t*)
            xstrdup((char*) association->instance_pattern);
        association->class_pattern = (utf8_t*)
            xstrdup((char*) association->class_pattern);
        association->actions = duplicate_actions(association->actions,
                association->number_of_actions);
    }
}

/* Copy the button bindings of @duplicate into itself. */
static void duplicate_configuration_button_bindings(
        struct configuration *duplicate)
{
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
        duplicate->font.name = (utf8_t*) xstrdup((char*) duplicate->font.name);
    }
    duplicate->startup.actions = duplicate_actions(duplicate->startup.actions,
            duplicate->startup.number_of_actions);
    duplicate_configuration_associations(duplicate);
    duplicate_configuration_button_bindings(duplicate);
    duplicate_configuration_key_bindings(duplicate);
}

/* Clear the resources given configuration occupies. */
void clear_configuration(struct configuration *configuration)
{
    free(configuration->font.name);

    free_actions(configuration->startup.actions,
            configuration->startup.number_of_actions);

    /* free associations */
    for (uint32_t i = 0;
            i < configuration->assignment.number_of_associations;
            i++) {
        struct configuration_association *const association =
            &configuration->assignment.associations[i];
        free(association->instance_pattern);
        free(association->class_pattern);
        free_actions(association->actions, association->number_of_actions);
    }
    free(configuration->assignment.associations);

    /* free button bindings */
    for (uint32_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        free_actions(configuration->mouse.buttons[i].actions,
                configuration->mouse.buttons[i].number_of_actions);
    }
    free(configuration->mouse.buttons);

    /* free key bindings */
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
        path = xasprintf("%s/%s", fensterchef_home,
                &fensterchef_configuration[2]);
    } else {
        path = xstrdup(fensterchef_configuration);
    }

    if (load_configuration(path, &configuration, true) == OK) {
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

/* Grab the mouse bindings for a window so we receive MousePress/MouseRelease
 * events for it.
 *
 * This once was different were the buttons were grabbed on the root but this
 * led to weird bugs which I can not explain that caused odd behaviours when
 * replaying events.
 */
void grab_configured_buttons(xcb_window_t window)
{
    struct configuration_button *button;

    /* ungrab all previous buttons so we can overwrite them */
    xcb_ungrab_button(connection, XCB_BUTTON_INDEX_ANY, window,
            XCB_BUTTON_MASK_ANY);

    for (uint32_t i = 0; i < configuration.mouse.number_of_buttons; i++) {
        button = &configuration.mouse.buttons[i];
        /* use every possible combination of modifiers we do not care about
         * so that when the user has CAPS LOCK for example, it does not mess
         * with mouse bindings
         *
         * TODO: is this worth just to avoid getting a few additional events
         * from the server?
         */
        for (uint32_t j = 0; j < (uint32_t) (1 << 8); j++) {
            /* check if @j has any outside modifiers */
            if ((j | configuration.mouse.ignore_modifiers) !=
                    configuration.mouse.ignore_modifiers) {
                continue;
            }

            xcb_grab_button(connection,
                    false, /* report all events with respect to `window` */
                    window, /* this is the window we grab the button for */
                    /* TODO: specifying the ButtonPressMask makes no
                     * difference, figure out why and who is responsible for
                     * that
                     */
                    (button->flags & BINDING_FLAG_RELEASE) ?
                    XCB_EVENT_MASK_BUTTON_RELEASE : XCB_EVENT_MASK_BUTTON_PRESS,
                    /* SYNC means that pointer (mouse) events will be frozen
                     * until we issue a AllowEvents request; this allows us to
                     * make the decision to either drop the event or send it on
                     * to the actually pressed client
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

/* Get a configured key from key modifiers and a key code. */
struct configuration_key *find_configured_key_by_code(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keycode_t key_code, uint16_t flags)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching key (the key code AND modifiers must match up) */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->modifiers == modifiers &&
                (key->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            if (key->key_code == key_code || (key->key_symbol != XCB_NONE &&
                    get_keysym(key_code) == key->key_symbol)) {
                return key;
            }
        }
    }
    return NULL;
}

/* Get a configured key from key modifiers and a key symbol. */
struct configuration_key *find_configured_key_by_symbol(
        struct configuration *configuration,
        uint16_t modifiers, xcb_keysym_t key_symbol, uint16_t flags)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching key (the key symbol AND modifiers must match up) */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->modifiers == modifiers &&
                (key->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            if (key->key_symbol == key_symbol || (key->key_symbol == XCB_NONE &&
                    get_keysym(key->key_code) == key_symbol)) {
                return key;
            }
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

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *new_configuration)
{
    xcb_render_color_t color;

    /* replace the configuration */
    clear_configuration(&configuration);
    configuration = *new_configuration;

    /* reload all X cursors and cursor themes */
    reload_resources();

    /* set the root cursor */
    general_values[0] = load_cursor(configuration.general.root_cursor);
    xcb_change_window_attributes(connection, screen->root, XCB_CW_CURSOR,
            general_values);

    /* reload the font */
    if (configuration.font.name != NULL) {
        set_font(configuration.font.name);
    }

    /* refresh the border size and color of all windows */
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (window == focus_window) {
            window->border_color = configuration.border.focus_color;
        } else {
            window->border_color = configuration.border.color;
        }
        if (has_window_border(window)) {
            window->border_size = configuration.border.size;
        }
    }

    /* reload all frames since the gaps or border sizes might have changed */
    for (Monitor *monitor = first_monitor; monitor != NULL;
            monitor = monitor->next) {
        resize_frame_and_ignore_ratio(monitor->frame, monitor->frame->x,
                monitor->frame->y, monitor->frame->width,
                monitor->frame->height);
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

    /* change the text colors */
    if (stock_objects[STOCK_WHITE_PEN] != XCB_NONE) {
        convert_color_to_xcb_color(&color,
                configuration.notification.background);
        set_pen_color(stock_objects[STOCK_WHITE_PEN], color);
    }
    if (stock_objects[STOCK_BLACK_PEN] != XCB_NONE) {
        convert_color_to_xcb_color(&color,
                configuration.notification.foreground);
        set_pen_color(stock_objects[STOCK_BLACK_PEN], color);
    }

    /* change the background colors */
    if (stock_objects[STOCK_GC] != XCB_NONE &&
            stock_objects[STOCK_INVERTED_GC] != XCB_NONE) {
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
    for (Window *window = first_window; window != NULL; window = window->next) {
        grab_configured_buttons(window->client.id);
    }
    grab_configured_keys();
}

/* Load a configuration from a string or file. */
int load_configuration(const char *string,
        struct configuration *destination_configuration,
        bool load_from_file)
{
    Parser parser;
    parser_error_t error;

    memset(&parser, 0, sizeof(parser));

    /* either load from a file or a string source */
    if (load_from_file) {
        parser.file = fopen(string, "r");
        if (parser.file == NULL) {
            LOG_ERROR("could not open configuration file %s: %s\n",
                    string, strerror(errno));
            return ERROR;
        }
    } else {
        parser.string_source = string;
    }

    parser.line_capacity = 128;
    parser.line = xmalloc(parser.line_capacity);

    parser.configuration = default_configuration;
    duplicate_configuration(&parser.configuration);

    /* parse line by line */
    while (read_next_line(&parser)) {
        error = parse_line(&parser);
        /* emit an error if a good line has any trailing characters */
        if (error == PARSER_SUCCESS && parser.line[parser.column] != '\0') {
            error = PARSER_ERROR_TRAILING;
        }

        if (error != PARSER_SUCCESS) {
            if (load_from_file) {
                LOG_ERROR("%s:%zu: %s\n", string, parser.line_number,
                        parser_error_to_string(error));
            } else {
                LOG_ERROR("%zu: %s\n", parser.line_number,
                        parser_error_to_string(error));
            }
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

    if (parser.file != NULL) {
        fclose(parser.file);
    }

    if (error != PARSER_SUCCESS) {
        clear_configuration(&parser.configuration);
        return ERROR;
    }

    /* set the existing button bindings if no mouse section is specified */
    if (!parser.has_label[PARSER_LABEL_MOUSE]) {
        parser.configuration.mouse.buttons = configuration.mouse.buttons;
        parser.configuration.mouse.number_of_buttons =
            configuration.mouse.number_of_buttons;
        duplicate_configuration_button_bindings(&parser.configuration);
    }

    /* set the existing key bindings if no keyboard section is specified */
    if (!parser.has_label[PARSER_LABEL_KEYBOARD]) {
        parser.configuration.keyboard.keys = configuration.keyboard.keys;
        parser.configuration.keyboard.number_of_keys =
            configuration.keyboard.number_of_keys;
        duplicate_configuration_key_bindings(&parser.configuration);
    }

    if (load_from_file) {
        LOG("successfully read configuration file %s\n", string);
    }

    *destination_configuration = parser.configuration;

    return OK;
}
