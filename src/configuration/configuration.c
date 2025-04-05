#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "configuration/parser.h"
#include "fensterchef.h"
#include "font.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "notification.h"
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
    for (size_t i = 0;
            i < duplicate->assignment.number_of_associations;
            i++) {
        struct configuration_association *const association =
            &duplicate->assignment.associations[i];
        association->instance_pattern = xstrdup(association->instance_pattern);
        association->class_pattern = xstrdup(association->class_pattern);
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
    for (size_t i = 0; i < duplicate->mouse.number_of_buttons; i++) {
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
    for (size_t i = 0; i < duplicate->keyboard.number_of_keys; i++) {
        struct configuration_key *const key = &duplicate->keyboard.keys[i];
        key->expression.instructions = DUPLICATE(
                key->expression.instructions,
                key->expression.instruction_size);
    }
}

/* Create a deep copy of @duplicate and put it into itself. */
void duplicate_configuration(struct configuration *duplicate)
{
    duplicate->font.name = xstrdup(duplicate->font.name);
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
    for (size_t i = 0;
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
    for (size_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        free(configuration->mouse.buttons[i].expression.instructions);
    }
    free(configuration->mouse.buttons);

    /* free key bindings */
    for (size_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        free(configuration->keyboard.keys[i].expression.instructions);
    }
    free(configuration->keyboard.keys);
}

/* Load the user configuration and make it the current configuration. */
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
        unsigned modifiers, unsigned button_index, unsigned flags)
{
    struct configuration_button *button;

    /* remove the ignored modifiers */
    modifiers &= ~configuration->mouse.ignore_modifiers;
    /* remove all the button modifiers so that release events work properly */
    modifiers &= 0xff;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching button (the button AND modifiers must match up) */
    for (size_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        button = &configuration->mouse.buttons[i];
        if (button->modifiers == modifiers && button->index == button_index &&
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
void grab_configured_buttons(Window window)
{
    struct configuration_button *button;

    /* ungrab all previous buttons so we can overwrite them */
    XUngrabButton(display, AnyButton, AnyModifier, window);

    for (size_t i = 0; i < configuration.mouse.number_of_buttons; i++) {
        button = &configuration.mouse.buttons[i];
        /* use every possible combination of modifiers we do not care about
         * so that when the user has CAPS LOCK for example, it does not mess
         * with mouse bindings
         */
        for (unsigned j = configuration.mouse.ignore_modifiers;
                true;
                j = ((j - 1) & configuration.mouse.ignore_modifiers)) {
            XGrabButton(display, button->index, (j | button->modifiers),
                    window, /* this is the window we grab the button for */
                    True, /* report all events with respect to `window` */
                    (button->flags & BINDING_FLAG_RELEASE) ?
                        ButtonPressMask | ButtonReleaseMask :
                        ButtonPressMask,
                    /* SYNC means that pointer (mouse) events will be frozen
                     * until we issue a AllowEvents request; this allows us to
                     * make the decision to either drop the event or send it on
                     * to the actually pressed client
                     */
                    GrabModeSync,
                    /* do not freeze keyboard events */
                    GrabModeAsync,
                    None, /* no confinement of the pointer */
                    None /* no change of cursor */);

            if (j == 0) {
                break;
            }
        }
    }
}

/* Get a configured key from key modifiers and a key code. */
struct configuration_key *find_configured_key(
        struct configuration *configuration,
        unsigned modifiers, KeyCode key_code, unsigned flags)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching key (the key symbol AND modifiers must match up) */
    for (size_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->modifiers == modifiers && key->key_code == key_code &&
                (key->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            return key;
        }
    }
    return NULL;
}

/* Get a configured key from key modifiers and a key symbol. */
struct configuration_key *find_configured_key_by_key_symbol(
        struct configuration *configuration,
        unsigned modifiers, KeySym key_symbol, unsigned flags)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;
    flags &= ~BINDING_FLAG_TRANSPARENT;

    /* find a matching key (the key symbol AND modifiers must match up) */
    for (size_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->key_symbol == NoSymbol) {
            continue;
        }
        if (key->modifiers == modifiers && key->key_symbol == key_symbol &&
                (key->flags & ~BINDING_FLAG_TRANSPARENT) == flags) {
            return key;
        }
    }
    return NULL;
}

/* Grab the key bindings so we receive KeyPressed/KeyReleased events for them.
 */
void grab_configured_keys(void)
{
    Window root;
    const struct configuration_key *key;

    root = DefaultRootWindow(display);

    /* remove all previously grabbed keys so that we can overwrite them */
    XUngrabKey(display, AnyKey, AnyModifier, root);

    /* re-compute any key codes */
    for (size_t i = 0; i < configuration.keyboard.number_of_keys; i++) {
        struct configuration_key *const key = &configuration.keyboard.keys[i];
        key->key_code = XKeysymToKeycode(display, key->key_symbol);
    }

    for (size_t i = 0; i < configuration.keyboard.number_of_keys; i++) {
        key = &configuration.keyboard.keys[i];
        /* iterate over all bit combinations */
        for (unsigned j = configuration.keyboard.ignore_modifiers;
                true;
                j = ((j - 1) & configuration.keyboard.ignore_modifiers)) {
            XGrabKey(display, key->key_code, (j | key->modifiers),
                    root, /* the window to grab the keys for */
                    True, /* report the event relative to the root */
                    /* do not freeze pointer (mouse) events */
                    GrabModeAsync,
                    /* do not freeze keyboard events */
                    GrabModeAsync);

            if (j == 0) {
                break;
            }
        }
    }
}

/* Compare the current configuration with the new configuration and set it. */
void set_configuration(struct configuration *new_configuration)
{
    /* replace the configuration */
    clear_configuration(&configuration);
    configuration = *new_configuration;

    /* set the root cursor */
    XDefineCursor(display, DefaultRootWindow(display),
            load_cursor(configuration.general.root_cursor));

    /* reload the font */
    if (configuration.font.name != NULL) {
        set_font(configuration.font.name);
    }

    /* refresh the border size and color of all windows */
    for (FcWindow *window = Window_first; window != NULL; window = window->next) {
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

    /* re-grab all bindings */
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
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
                for (unsigned i = 0; i < parser.column; i++) {
                    fprintf(stderr, " ");
                }
                fprintf(stderr, "^");
                for (unsigned i = parser.column + 1;
                        parser.line[i] != '\0'; i++) {
                    fprintf(stderr, "~");
                }
                fprintf(stderr, "\n");
            } else {
                /* indicate the error region using "  ~~~^" */
                for (unsigned i = 0; i < parser.item_start_column; i++) {
                    fprintf(stderr, " ");
                }
                for (unsigned i = parser.item_start_column + 1;
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
