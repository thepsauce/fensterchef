#include <ctype.h>
#include <string.h>

#include <X11/Xlib.h>

#include "action.h"
#include "configuration.h"
#include "parse/parse.h"
#include "parse/struct.h"
#include "parse/stream.h"
#include "parse/utility.h"
#include "cursor.h"
#include "font.h"
#include "log.h"
#include "window.h"
#include "x11_synchronize.h"

/* the parser struct */
struct parser parser;

/* Emit a parse error. */
void emit_parse_error(const char *message)
{
    unsigned line, column;
    const utf8_t *file;
    const char *string_line;
    unsigned length;

    parser.error_count++;
    get_stream_position(parser.stream, parser.index, &line, &column);
    string_line = get_stream_line(parser.stream, line, &length);

    file = parser.stream->file_path;
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
        struct window_association association;

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
    button_t button_index;
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
            character = peek_stream_character(parser.stream),
            character == '+') {
        /* skip over '+' */
        (void) get_stream_character(parser.stream);

        if (resolve_integer() != OK) {
            emit_parse_error("invalid integer value");
        }
        modifiers |= parser.data.u.integer;
        assert_read_string();
        has_anything = true;
    }

    button_index = translate_string_to_button(parser.string);
    if (button_index == BUTTON_NONE) {
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

    if (button_index != BUTTON_NONE) {
        struct button_binding button;
        struct action_list_item item;
        struct parse_generic_data data;

        button.is_release = is_release;
        button.is_transparent = is_transparent;
        button.modifiers = modifiers;
        button.button = button_index;
        LIST_COPY_ALL(parser.action_items, button.actions.items);
        button.actions.number_of_items = parser.action_items_length;
        LIST_COPY_ALL(parser.action_data, button.actions.data);

        /* set all to zero, ready for the next action */
        LIST_SET(parser.action_data, 0, NULL, parser.action_data_length);

        item.type = ACTION_BUTTON_BINDING;
        item.data_count = 1;
        LIST_APPEND_VALUE(parser.startup_items, item);

        data.flags = 0;
        data.type = PARSE_DATA_TYPE_BUTTON;
        data.u.button = button;
        LIST_APPEND_VALUE(parser.startup_data, data);
    } else {
        if (is_transparent) {
            parser.index = transparent_position;
            emit_parse_error("key bindings do not support 'transparent'");
        } else {
            struct key_binding key;
            struct action_list_item item;
            struct parse_generic_data data;

            key.is_release = is_release;
            key.modifiers = modifiers;
            key.key_symbol = key_symbol;
            key.key_code = key_code;
            LIST_COPY_ALL(parser.action_items, key.actions.items);
            key.actions.number_of_items = parser.action_items_length;
            LIST_COPY_ALL(parser.action_data, key.actions.data);

            /* set all to zero, ready for the next action */
            LIST_SET(parser.action_data, 0, NULL, parser.action_data_length);

            item.type = ACTION_KEY_BINDING;
            item.data_count = 1;
            LIST_APPEND_VALUE(parser.startup_items, item);

            data.flags = 0;
            data.type = PARSE_DATA_TYPE_KEY;
            data.u.key = key;
            LIST_APPEND_VALUE(parser.startup_data, data);
        }
    }
}

/* Parse the currently active stream. */
int parse_stream(InputStream *stream)
{
    int character;

    parser.stream = stream;
    /* clear an old parser */
    parser.error_count = 0;
    parser.startup_items_length = 0;
    parser.startup_data_length = 0;
    parser.associations_length = 0;

    /* make it so `throw_parse_error()` jumps to after this statement */
    (void) setjmp(parser.throw_jump);

    if (parser.error_count >= PARSE_MAX_ERROR_COUNT) {
        emit_parse_error("parsing stopped: too many errors occured");
        return ERROR;
    }

    while (skip_space(), character = peek_stream_character(parser.stream),
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
int parse_stream_and_run_actions(InputStream *stream)
{
    struct action_list startup;

    if (parse_stream(stream) != OK) {
        return ERROR;
    }

    add_window_associations(parser.associations, parser.associations_length);

    /* do the startup actions */
    startup.items = parser.startup_items;
    startup.number_of_items = parser.startup_items_length;
    startup.data = parser.startup_data;
    LOG_DEBUG("running startup actions: %A\n",
            &startup);
    run_action_list(&startup);

    /* keep shallow elements so we can recycle the memory */
    clear_action_list_but_keep_shallow(&startup);

    return OK;
}

/* Parse the currently active stream and use it to override the configuration. */
int parse_stream_and_replace_configuration(InputStream *stream)
{
    struct action_list startup;

    if (parse_stream(stream) != OK) {
        return ERROR;
    }

    /* clear the old configuration */
    clear_configuration();

    add_window_associations(parser.associations, parser.associations_length);

    /* do the startup actions */
    startup.items = parser.startup_items;
    startup.number_of_items = parser.startup_items_length;
    startup.data = parser.startup_data;
    LOG_DEBUG("running startup actions: %A\n",
            &startup);
    run_action_list(&startup);

    /* keep shallow elements so we can recycle the memory */
    clear_action_list_but_keep_shallow(&startup);

    return OK;
}
