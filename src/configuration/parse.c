#include <ctype.h>
#include <string.h>

#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "configuration/action.h"
#include "configuration/configuration.h"
#include "configuration/literal.h"
#include "configuration/parse.h"
#include "configuration/parse_struct.h"
#include "configuration/stream.h"
#include "cursor.h"
#include "font.h"
#include "log.h"
#include "window.h"
#include "x11_management.h"

/* the parser struct */
struct parser parser;

/* the corresponding string identifier for all actions */
const char *action_strings[ACTION_MAX] = {
#define X(identifier, string) \
    [identifier] = string,
    DEFINE_ALL_PARSE_ACTIONS
#undef X
};

/* Forward declaration... */
static int continue_parsing_action(void);

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

/* Find a section in the action strings that match the word loaded into
 * `parser`.
 *
 * @return ERROR if no action matches.
 */
static int resolve_action_word(void)
{
    action_type_t count = 0;

    for (action_type_t i = 0; i < ACTION_MAX; i++) {
        const char *action, *space;
        unsigned skip_length;

        action = action_strings[i];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            skip_length = strlen(action);
            space = action + skip_length;
        } else {
            skip_length = space - action + 1;
        }

        if (parser.string_length == (size_t) (space - action) &&
                memcmp(action, parser.string,
                    parser.string_length) == 0) {
            if (count == 0) {
                parser.first_action = i;
            }

            count++;

            parser.last_action = i + 1;

            parser.actions[i].offset = skip_length;
            /* prepare the data for filling */
            parser.actions[i].data_length = 0;
        } else if (count > 0) {
            /* optimization: no more will match because of the alphabetical
             * sorting
             */
            break;
        }
    }

    return count == 0 ? ERROR : OK;
}

/* Read the next word and find the actions where the word matches.
 *
 * @return ERROR if no action matches.
 */
static int read_and_resolve_next_action_word(void)
{
    action_type_t count = 0;

    assert_read_string();

    /* go through all actions that matched previously */
    for (action_type_t i = parser.first_action, end = parser.last_action;
            i < end;
            i++) {
        const char *action, *space;
        unsigned skip_length;

        if (parser.actions[i].offset == -1) {
            continue;
        }

        action = &action_strings[i][parser.actions[i].offset];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            skip_length = strlen(action);
            space = action + skip_length;
        } else {
            skip_length = space - action + 1;
        }

        /* check if next is a string parameter */
        if (action[0] == 'S') {
            /* append the string data point */
            parser.data.flags = PARSE_DATA_FLAGS_IS_POINTER;
            LIST_COPY(parser.string, 0, parser.string_length + 1,
                    parser.data.u.string);
            LIST_APPEND_VALUE(parser.actions[i].data, parser.data);
        } else {
            /* the word needs to be unquoted */
            if (parser.is_string_quoted) {
                parser.actions[i].offset = -1;
                continue;
            }

            /* check if an integer is expected and try to resolve it */
            if (action[0] == 'I') {
                if (resolve_integer() == OK) {
                    /* append the integer data point */
                    LIST_APPEND_VALUE(parser.actions[i].data, parser.data);
                } else {
                    parser.actions[i].offset = -1;
                    continue;
                }
            } else {
                /* try to match the next word */
                if (parser.string_length != (size_t) (space - action) ||
                        memcmp(action, parser.string,
                            parser.string_length) != 0) {
                    parser.actions[i].offset = -1;
                    continue;
                }
            }
        }

        /* got a valid action */

        if (count == 0) {
            parser.first_action = i;
        }
        count++;

        parser.last_action = i + 1;

        parser.actions[i].offset += skip_length;
    }

    return count == 0 ? ERROR : OK;
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

/* Print all possible actions to stderr. */
static void print_action_possibilities(void)
{
    fprintf(stderr, "possible words are: ");
    for (action_type_t i = parser.first_action; i < parser.last_action; i++) {
        const char *action, *space;
        int length;

        action = &action_strings[i][parser.actions[i].offset];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            length = strlen(action);
            space = action + length;
        } else {
            length = space - action;
        }

        if (i != parser.first_action) {
            fprintf(stderr, ", ");
        }
        if (action[0] == 'I') {
            fprintf(stderr, "INTEGER");
        } else if (action[0] == 'S') {
            fprintf(stderr, "STRING");
        } else {
            fprintf(stderr, "%.*s",
                    length, action);
        }
    }
    fprintf(stderr, "\n");
}

/* Parse the next action word or check for an action separator.
 *
 * @return ERROR when there are no following actions, otherwise OK.
 */
static int parse_next_action_part(size_t item_index)
{
    int error = OK;
    int character;

    character = peek_stream_character();
    if (character == EOF || character == ',' || character == '\n') {
        if (action_strings[parser.first_action]
                [parser.actions[parser.first_action].offset] != '\0') {
            parser.index = input_stream.index;
            emit_parse_error("incomplete action");
            print_action_possibilities();
        } else {
            parser.action_items[item_index].type = parser.first_action;
            parser.action_items[item_index].data_count =
                parser.actions[parser.first_action].data_length;
            LIST_APPEND(parser.action_data,
                parser.actions[parser.first_action].data,
                parser.actions[parser.first_action].data_length);
        }

        if (character == ',') {
            /* skip ',' */
            (void) get_stream_character();
            skip_space();
            assert_read_string();
        } else {
            error = ERROR;
        }
    } else {
        if (read_and_resolve_next_action_word() == ERROR) {
            skip_statement();
            emit_parse_error("invalid action word");
            error = ERROR;
        } else {
            error = parse_next_action_part(item_index);
        }
    }

    return error;
}

/* Parse an action.
 *
 * Expects that a string has been read into `parser.`
 *
 * @return ERROR if there is no action, OK otherwise.
 */
static int continue_parsing_action(void)
{
    size_t item_index;

    parser.action_items_length = 0;
    parser.action_data_length = 0;

    do {
        item_index = parser.action_items_length;
        LIST_APPEND(parser.action_items, NULL, 1);

        if (resolve_action_word() != OK) {
            /* the action might be a binding or association */
            return ERROR;
        }
    } while (parse_next_action_part(item_index) == OK);

    return OK;
}

/* Translate a string to some extended key symbols. */
static KeySym translate_string_to_additional_key_symbols(const char *string)
{
    struct {
        const char *name;
        KeySym key_symbol;
    } symbol_table[] = {
        /* since integers are interpreted as keycodes, these are needed to still
         * use the digit keys
         */
        { "zero", XK_0 },
        { "one", XK_1 },
        { "two", XK_2 },
        { "three", XK_3 },
        { "four", XK_4 },
        { "five", XK_5 },
        { "six", XK_6 },
        { "seven", XK_7 },
        { "eight", XK_8 },
        { "nine", XK_9 },
    };

    for (unsigned i = 0; i < SIZE(symbol_table); i++) {
        if (strcmp(string, symbol_table[i].name) == 0) {
            return symbol_table[i].key_symbol;
        }
    }

    return NoSymbol;
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

    printf("bind %d%d %u + %s ...\n",
            is_release, is_transparent, modifiers, parser.string);

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
