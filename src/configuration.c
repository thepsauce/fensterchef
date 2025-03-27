#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "parser.h"
#include "render.h"
#include "resources.h"
#include "size_frame.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"

/* the currently loaded configuration */
struct configuration configuration;

/* Copy the associations of @duplicate into itself. */
static void duplicate_configuration_associations(
        struct configuration *duplicate)
{
    duplicate->assignment.associations = DUPLICATE(
            duplicate->assignment.associations,
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
        association->expression.instructions = DUPLICATE(
                association->expression.instructions,
                association->expression.instruction_size);
    }
}

/* Copy the button bindings of @duplicate into itself. */
static void duplicate_configuration_button_bindings(
        struct configuration *duplicate)
{
    duplicate->mouse.buttons = DUPLICATE(
            duplicate->mouse.buttons,
            duplicate->mouse.number_of_buttons);
    for (uint32_t i = 0; i < duplicate->mouse.number_of_buttons; i++) {
        struct configuration_button *const button =
            &duplicate->mouse.buttons[i];
        button->expression.instructions = DUPLICATE(
                button->expression.instructions,
                button->expression.instruction_size);
    }
}

/* Copy the key bindings of @duplicate into itself. */
static void duplicate_configuration_key_bindings(
        struct configuration *duplicate)
{
    duplicate->keyboard.keys = DUPLICATE(
            duplicate->keyboard.keys,
            duplicate->keyboard.number_of_keys);
    for (uint32_t i = 0; i < duplicate->keyboard.number_of_keys; i++) {
        struct configuration_key *const key = &duplicate->keyboard.keys[i];
        key->expression.instructions = DUPLICATE(
                key->expression.instructions,
                key->expression.instruction_size);
    }
}

/* Create a deep copy of @duplicate and put it into itself. */
void duplicate_configuration(struct configuration *duplicate)
{
    duplicate->font.name = (utf8_t*) xstrdup((char*) duplicate->font.name);
    duplicate->startup.expression.instructions = DUPLICATE(
            duplicate->startup.expression.instructions,
            duplicate->startup.expression.instruction_size);
    duplicate_configuration_associations(duplicate);
    duplicate_configuration_button_bindings(duplicate);
    duplicate_configuration_key_bindings(duplicate);
}

/* Clear the resources given configuration occupies. */
void clear_configuration(struct configuration *configuration)
{
    free(configuration->font.name);

    free(configuration->startup.expression.instructions);

    /* free associations */
    for (uint32_t i = 0;
            i < configuration->assignment.number_of_associations;
            i++) {
        struct configuration_association *const association =
            &configuration->assignment.associations[i];
        free(association->instance_pattern);
        free(association->class_pattern);
        free(association->expression.instructions);
    }
    free(configuration->assignment.associations);

    /* free button bindings */
    for (uint32_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        free(configuration->mouse.buttons[i].expression.instructions);
    }
    free(configuration->mouse.buttons);

    /* free key bindings */
    for (uint32_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        free(configuration->keyboard.keys[i].expression.instructions);
    }
    free(configuration->keyboard.keys);
}

/* Load the user configuration and merge it into the current configuration. */
void reload_user_configuration(void)
{
    struct configuration configuration;

    if (load_configuration(Fensterchef_configuration, &configuration,
                true) == OK) {
        set_configuration(&configuration);
    }
}

/* Get a key from button modifiers and a button index. */
struct configuration_button *find_configured_button(
        struct configuration *configuration,
        uint16_t modifiers, xcb_button_t button_index, uint16_t flags)
{
    struct configuration_button *button;

    /* remove the ignored modifiers */
    modifiers &= ~configuration->mouse.ignore_modifiers;
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
    uint16_t modifiers;

    /* ungrab all previous buttons so we can overwrite them */
    xcb_ungrab_button(connection, XCB_BUTTON_INDEX_ANY, window,
            XCB_BUTTON_MASK_ANY);

    for (uint32_t i = 0; i < configuration.mouse.number_of_buttons; i++) {
        button = &configuration.mouse.buttons[i];
        /* use every possible combination of modifiers we do not care about
         * so that when the user has CAPS LOCK for example, it does not mess
         * with mouse bindings
         */
        for (uint16_t j = 0; j <= configuration.mouse.ignore_modifiers; j++) {
            /* check if @j has any outside modifiers */
            if ((j | configuration.mouse.ignore_modifiers) !=
                    configuration.mouse.ignore_modifiers) {
                continue;
            }

            modifiers = (j | button->modifiers) & 0xff;

            xcb_grab_button(connection,
                    true, /* report all events with respect to `window` */
                    window, /* this is the window we grab the button for */
                    /* TODO: specifying the ButtonPressMask makes no
                     * difference, figure out why and who is responsible for
                     * that
                     */
                    (button->flags & BINDING_FLAG_RELEASE) ?
                        XCB_EVENT_MASK_BUTTON_RELEASE :
                        XCB_EVENT_MASK_BUTTON_PRESS,
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
                    button->index, modifiers);
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

/* Grab the key bindings so we receive the KeyPress/KeyRelease events for them.
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
            for (uint16_t k = 0;
                    k <= configuration.mouse.ignore_modifiers;
                    k++) {
                /* check if @k has any outside modifiers */
                if ((k | configuration.keyboard.ignore_modifiers) !=
                        configuration.keyboard.ignore_modifiers) {
                    continue;
                }

                modifiers = (k | configuration.keyboard.keys[i].modifiers);

                xcb_grab_key(connection,
                        true, /* report the event relative to the root */
                        root, /* the window to grab the keys for */
                        modifiers, keycodes[j],
                        /* do not freeze pointer (mouse) events */
                        XCB_GRAB_MODE_ASYNC,
                        /* do not freeze keyboard events */
                        XCB_GRAB_MODE_ASYNC);
            }
        }
        free(keycodes);
    }
}

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *new_configuration)
{
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
        set_modern_font(configuration.font.name);
    }

    /* refresh the border size and color of all windows */
    for (Window *window = Window_first; window != NULL; window = window->next) {
        if (window == Window_focus) {
            window->border_color = configuration.border.focus_color;
        } else {
            window->border_color = configuration.border.color;
        }
        if (!is_window_borderless(window)) {
            window->border_size = configuration.border.size;
        }
    }

    /* reload all frames since the gaps or border sizes might have changed */
    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        resize_frame_and_ignore_ratio(monitor->frame, monitor->frame->x,
                monitor->frame->y, monitor->frame->width,
                monitor->frame->height);
    }

    /* change border color and size of the notification window */
    change_client_attributes(&notification,
            configuration.notification.background,
            configuration.notification.border_color);
    configure_client(&notification, notification.x, notification.y,
            notification.width, notification.height,
            configuration.notification.border_size);

    /* change border color and size of the window list window */
    change_client_attributes(&window_list.client,
            configuration.notification.background,
            configuration.notification.border_color);
    configure_client(&window_list.client, window_list.client.x,
            window_list.client.y, window_list.client.width,
            window_list.client.height, configuration.notification.border_size);

    /* re-grab all bindings */
    for (Window *window = Window_first; window != NULL; window = window->next) {
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
    parser_error_t error = PARSER_SUCCESS;

    if (initialize_parser(&parser, string, load_from_file) != OK) {
        LOG_ERROR("could not open configuration file %s: %s\n",
                string, strerror(errno));
        return ERROR;
    }

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
            if (parser.number_of_pushed_files > 0) {
                LOG_ERROR("%s:%zu: %s\n", parser.file_stack[
                            parser.number_of_pushed_files - 1].name,
                        parser.line_number,
                        parser_error_to_string(error));
            } else if (load_from_file) {
                LOG_ERROR("%s:%zu: %s\n", string,
                        parser.line_number,
                        parser_error_to_string(error));
            } else {
                LOG_ERROR("%zu: %s\n",
                        parser.line_number,
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

    deinitialize_parser(&parser);

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
