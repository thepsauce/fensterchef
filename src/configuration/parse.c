#include <ctype.h>
#include <string.h>

#include <X11/Xlib.h>

#include "configuration/action.h"
#include "configuration/configuration.h"
#include "configuration/parse.h"
#include "configuration/parse_struct.h"
#include "configuration/stream.h"
#include "configuration/utility.h"
#include "cursor.h"
#include "font.h"
#include "log.h"
#include "window.h"
#include "x11_management.h"

/* the parser struct */
struct parser parser;

/* Emit a parse error. */
void emit_parse_error(const char *message)
{
    unsigned line, column;
    const utf8_t *file;
    char *string_line;
    unsigned length;

    parser.error_count++;
    get_stream_position(parser.index, &line, &column);
    string_line = get_stream_line(line, &length);

    file = input_stream.file_path;
    if (file == NULL) {
        file = "<string>";
    }

    LOG_ERROR("%s:%d:%d: %s\n",
            file, line + 1, column + 1, message);
    fprintf(stderr, "%.*s\n", (int) length, string_line);
    for (unsigned i = 0; i < column; i++) {
        fputc(' ', stderr);
    }
    fputs("^\n", stderr);
}

/* Parse the next assigment in the active stream.
 *
 * This assumes a string has been read into the parser.
 */
static void continue_parsing_association(void)
{
    utf8_t *next, *separator;
    size_t length;
    int backslash_count;

    next = parser.string;
    length = parser.string_length;
    /* handle any comma "," that is escaped by a backslash "\" */
    do {
        separator = memchr(next, ',', length);
        if (separator == NULL) {
            break;
        }

        length -= separator - next + 1;
        next = separator + 1;

        backslash_count = 0;
        for (; separator > parser.string; separator--) {
            if (separator[-1] != '\\') {
                break;
            }
            backslash_count++;
        }
    } while (backslash_count % 2 == 1);

    if (separator == NULL) {
        parser.instance_pattern_length = 0;
    } else {
        LIST_SET(parser.instance_pattern, 0, parser.string,
                separator - parser.string);
        LIST_APPEND_VALUE(parser.instance_pattern, '\0');
    }
    LIST_SET(parser.class_pattern, 0, next, length);
    LIST_APPEND_VALUE(parser.class_pattern, '\0');

    assert_read_string();
    if (parser.is_string_quoted) {
        emit_parse_error("expected word and not a string for association");
        skip_all_statements();
    } else if (continue_parsing_action() == OK) {
        struct configuration_association association;

        if (parser.instance_pattern_length == 0) {
            association.instance_pattern = NULL;
        } else {
            LIST_COPY_ALL(parser.instance_pattern,
                    association.instance_pattern);
        }
        LIST_COPY_ALL(parser.class_pattern, association.class_pattern);

        LIST_COPY_ALL(parser.action_items, association.actions.items);
        association.actions.number_of_items = parser.action_items_length;
        LIST_COPY_ALL(parser.action_data, association.actions.data);

        /* set all to zero */
        LIST_SET(parser.action_data, 0, NULL, parser.action_data_length);

        LIST_APPEND_VALUE(parser.associations, association);
    } else {
        emit_parse_error("invalid action word");
        skip_all_statements();
    }
}

/* Parse the next binding definition in `parser`.
 *
 * Expects that a string has been read into `parser.`
 */
static void continue_parsing_modifiers_or_binding(void)
{
    bool has_anything = false;
    bool is_release = false;
    bool is_transparent = false;
    int character;
    unsigned modifiers = 0;
    int button_index;
    KeySym key_symbol = NoSymbol;
    KeyCode key_code = 0;
    /* position for error reporting */
    size_t transparent_position = 0;

    if (strcmp(parser.string, "release") == 0) {
        assert_read_string();
        is_release = true;
        has_anything = true;
    }

    if (strcmp(parser.string, "transparent") == 0) {
        assert_read_string();
        transparent_position = parser.index;
        is_transparent = true;
        has_anything = true;
    }

    /* read any modifiers */
    while (skip_blanks(),
            character = peek_stream_character(),
            character == '+') {
        /* skip over '+' */
        (void) get_stream_character();

        if (resolve_integer() != OK) {
            emit_parse_error("invalid integer value");
        }
        modifiers |= parser.data.u.integer;
        assert_read_string();
        has_anything = true;
    }

    button_index = translate_string_to_button(parser.string);
    if (button_index == -1) {
        if (resolve_integer() == OK) {
            int min_key_code, max_key_code;

            /* for testing code, the display is NULL */
            if (UNLIKELY(display != NULL)) {
                XDisplayKeycodes(display, &min_key_code, &max_key_code);
                key_code = parser.data.u.integer;
                if (key_code < min_key_code || key_code > max_key_code) {
                    emit_parse_error("key code is out of range");
                }
            }
        } else {
            key_symbol = XStringToKeysym(parser.string);
            if (key_symbol == NoSymbol) {
                key_symbol = translate_string_to_additional_key_symbols(
                        parser.string);
                if (key_symbol == NoSymbol) {
                    if (has_anything) {
                        emit_parse_error(
                                "invalid button, key symbol or key code");
                    } else {
                        emit_parse_error("invalid action, button or key");
                        skip_all_statements();
                        return;
                    }
                }
            }
        }
    }

    assert_read_string();
    if (parser.is_string_quoted) {
        emit_parse_error("expected word and not a string for binding");
        skip_all_statements();
    } else if (continue_parsing_action() != OK) {
        emit_parse_error("invalid action");
        skip_all_statements();
    }

    if (button_index != -1) {
        struct configuration_button button;

        button.is_release = is_release;
        button.is_transparent = is_transparent;
        button.modifiers = modifiers;
        button.index = button_index;
        LIST_COPY_ALL(parser.action_items, button.actions.items);
        button.actions.number_of_items = parser.action_items_length;
        LIST_COPY_ALL(parser.action_data, button.actions.data);

        /* set all to zero */
        LIST_SET(parser.action_data, 0, NULL, parser.action_data_length);

        LIST_APPEND_VALUE(parser.buttons, button);
    } else {
        struct configuration_key key;

        if (is_transparent) {
            parser.index = transparent_position;
            emit_parse_error("key bindings do not support 'transparent'");
        } else {
            key.is_release = is_release;
            key.modifiers = modifiers;
            key.key_symbol = key_symbol;
            key.key_code = key_code;
            LIST_COPY_ALL(parser.action_items, key.actions.items);
            key.actions.number_of_items = parser.action_items_length;
            LIST_COPY_ALL(parser.action_data, key.actions.data);

            /* set all to zero */
            LIST_SET(parser.action_data, 0, NULL, parser.action_data_length);

            LIST_APPEND_VALUE(parser.keys, key);
        }
    }
}

/* Parse the currently active stream. */
int parse_stream(void)
{
    int character;

    /* clear an old parser */
    parser.error_count = 0;
    parser.startup_items_length = 0;
    parser.startup_data_length = 0;

    /* make it so `throw_parse_error()` jumps to after this statement */
    (void) setjmp(parser.throw_jump);

    if (parser.error_count >= PARSE_MAX_ERROR_COUNT) {
        emit_parse_error("parsing stopped: too many errors occured");
        return ERROR;
    }

    while (skip_space(), character = peek_stream_character(),
            character != EOF) {
        assert_read_string();

        if (parser.is_string_quoted) {
            continue_parsing_association();
        } else {
            if (continue_parsing_action() == OK) {
                LIST_APPEND(parser.startup_items,
                        parser.action_items, parser.action_items_length);
                LIST_APPEND(parser.startup_data,
                        parser.action_data, parser.action_data_length);
            } else {
                continue_parsing_modifiers_or_binding();
            }
        }
    }

    return parser.error_count > 0 ? ERROR : OK;
}

/* Parse the currently active stream and run all actions. */
int parse_stream_and_run_actions(void)
{
    struct action_list startup;

    if (parse_stream() != OK) {
        return ERROR;
    }

    /* do the startup actions */
    startup.items = parser.startup_items;
    startup.number_of_items = parser.startup_items_length;
    startup.data = parser.startup_data;
    do_action_list(&startup);
    clear_action_list(&startup);

    /* TODO: clear key bindings?? */
    /* TODO: clear button bindings?? */
    /* TODO: clear associations?? */

    return OK;
}

/* Parse the currently active stream and use it to override the configuration. */
int parse_stream_and_replace_configuration(void)
{
    struct action_list startup;

    if (parse_stream() != OK) {
        return ERROR;
    }

    /* free the old configuration */
    clear_configuration(&configuration);

    /* extract the associations and bindings into the new configuration */
    LIST_COPY_ALL(parser.associations, configuration.associations);
    configuration.number_of_associations = parser.associations_length;

    LIST_COPY_ALL(parser.keys, configuration.keys);
    configuration.number_of_keys = parser.keys_length;

    LIST_COPY_ALL(parser.buttons, configuration.buttons);
    configuration.number_of_buttons = parser.buttons_length;

    /* do the startup actions */
    startup.items = parser.startup_items;
    startup.number_of_items = parser.startup_items_length;
    startup.data = parser.startup_data;
    do_action_list(&startup);

    /* clear but keep shallow members */
    clear_action_list(&startup);

    /* re-grab all bindings */
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
        grab_configured_buttons(window->client.id);
    }
    grab_configured_keys();

    return OK;
}
