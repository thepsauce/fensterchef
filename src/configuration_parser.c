#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "cursor.h"
#include "fensterchef.h"
#include "configuration_parser.h"
#include "log.h"
#include "utility.h"
#include "x11_management.h"

/* conversion from parser error to string */
static const char *parser_error_strings[] = {
#define X(error, string) [error] = string,
    DEFINE_ALL_CONFIGURATION_PARSER_ERRORS
#undef X
};

/* conversion of string to modifier mask */
static const struct modifier_string {
    /* the string representation of the modifier */
    const char *name;
    /* the modifier bit */
    uint16_t modifier;
} modifier_strings[] = {
    { "None", 0 },

    { "Shift", XCB_MOD_MASK_SHIFT },
    { "Lock", XCB_MOD_MASK_LOCK },
    { "CapsLock", XCB_MOD_MASK_LOCK },
    { "Ctrl", XCB_MOD_MASK_CONTROL },
    { "Control", XCB_MOD_MASK_CONTROL },

    /* common synonyms for some modifiers */
    { "Alt", XCB_MOD_MASK_1 },
    { "Super", XCB_MOD_MASK_4 },

    { "Mod1", XCB_MOD_MASK_1 },
    { "Mod2", XCB_MOD_MASK_2 },
    { "Mod3", XCB_MOD_MASK_3 },
    { "Mod4", XCB_MOD_MASK_4 },
    { "Mod5", XCB_MOD_MASK_5 }

    /* since two bits are toggled this can not be a modifier */
#define INVALID_MODIFIER 3
};

/* conversion from string to button index */
static const struct button_string {
    /* the string representation of the button */
    const char *name;
    /* the button index */
    xcb_button_t button_index;
} button_strings[] = {
    /* buttons can also be Button<integer> to directly address the index */
    { "LButton", 1 },
    { "LeftButton", 1 },

    { "MButton", 2 },
    { "MiddleButton", 2 },

    { "RButton", 3 },
    { "RightButton", 3 },

    { "ScrollUp", 4 },
    { "WheelUp", 4 },

    { "ScrollDown", 5 },
    { "WheelDown", 5 },

    { "ScrollLeft", 6 },
    { "WheelLeft", 6 },

    { "ScrollRight", 7 },
    { "WheelRight", 7 },

#define FIRST_X_BUTTON 8
#define NUMBER_OF_X_BUTTONS 247
    /* X buttons (extra buttons on the mouse usually) going from X1 (8) to
     * X247 (255), they have their own handling and are not listed here
     */
};

/* The void data type is just a placeholder, it expects nothing. */
static parser_error_t parse_void(Parser *parser)
{
    (void) parser;
    return PARSER_SUCCESS;
}

/* Parse a boolean with some tolerance on the wording. */
static parser_error_t parse_boolean(Parser *parser);

/* Parse any text without leading or trailing space.
 *
 * This stops at a semicolon!
 */
static parser_error_t parse_string(Parser *parser);

/* Parse an integer in simple decimal notation. */
static parser_error_t parse_integer(Parser *parser);

/* Parse a set of 4 integers. */
static parser_error_t parse_quad(Parser *parser);

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static parser_error_t parse_color(Parser *parser);

/* Parse key modifiers, e.g.: Control+Shift. */
static parser_error_t parse_modifiers(Parser *parser);

/* Parse a cursor constant, e.g.: left-ptr. */
static parser_error_t parse_cursor(Parser *parser);

/* size in bytes of all data types */
static struct parser_data_type_information {
    /* size in bytes of the data type */
    size_t size;
    /* parse function for the data type */
    parser_error_t (*parse)(Parser *parser);
} data_types[] = {
    [PARSER_DATA_TYPE_VOID] = { 0, parse_void },
    [PARSER_DATA_TYPE_BOOLEAN] = {
        sizeof(((union parser_data_value*) 0)->boolean),
        parse_boolean
    },
    [PARSER_DATA_TYPE_STRING] = {
        sizeof(((union parser_data_value*) 0)->string),
        parse_string
    },
    [PARSER_DATA_TYPE_INTEGER] = {
        sizeof(((union parser_data_value*) 0)->integer),
        parse_integer
    },
    [PARSER_DATA_TYPE_QUAD] = {
        sizeof(((union parser_data_value*) 0)->quad),
        parse_quad
    },
    [PARSER_DATA_TYPE_COLOR] = {
        sizeof(((union parser_data_value*) 0)->color),
        parse_color
    },
    [PARSER_DATA_TYPE_MODIFIERS] = {
        sizeof(((union parser_data_value*) 0)->modifiers),
        parse_modifiers
    },
    [PARSER_DATA_TYPE_CURSOR] = {
        sizeof(((union parser_data_value*) 0)->cursor),
        parse_cursor
    }
};

#include "bits/configuration_parser_label_information.h"

/* Translate a string like "Button1" to a button index. */
static xcb_button_t translate_string_to_button(const char *string)
{
    /* parse indexes starting with "X" */
    if (tolower(string[0]) == 'x') {
        uint32_t x_index = 0;

        string++;
        while (isdigit(string[0])) {
            x_index *= 10;
            x_index += string[0] - '0';
            if (x_index > NUMBER_OF_X_BUTTONS) {
                return 0;
            }
            string++;
        }

        if (x_index == 0) {
            return 0;
        }

        return FIRST_X_BUTTON + x_index - 1;
    /* parse indexes starting with "button" */
    } else if (tolower(string[0]) == 'b' &&
            tolower(string[1]) == 'u' &&
            tolower(string[2]) == 't' &&
            tolower(string[3]) == 't' &&
            tolower(string[4]) == 'o' &&
            tolower(string[5]) == 'n') {
        uint32_t index = 0;

        string += strlen("button");
        while (isdigit(string[0])) {
            index *= 10;
            index += string[0] - '0';
            if (index > UINT8_MAX) {
                return 0;
            }
            string++;
        }
        return index;
    }

    for (uint32_t i = 0; i < SIZE(button_strings); i++) {
        if (strcasecmp(string, button_strings[i].name) == 0) {
            return button_strings[i].button_index;
        }
    }
    return 0;
}

/* Translate a string like "Shift" to a modifier bit. */
static uint16_t translate_string_to_modifier(const char *string)
{
    for (uint32_t i = 0; i < SIZE(modifier_strings); i++) {
        if (strcasecmp(modifier_strings[i].name, string) == 0) {
            return modifier_strings[i].modifier;
        }
    }
    return INVALID_MODIFIER;
}

/* Converts @error to a string. */
inline const char *parser_error_to_string(parser_error_t error)
{
    return parser_error_strings[error];
}

/* Read the next line from @parser->file into @parser->line. */
bool read_next_line(Parser *parser)
{
    size_t length;

    parser->line_number++;
    length = 0;
    for (int c;; ) {
        if (parser->file == NULL) {
            c = parser->string_source[parser->string_source_index];
            if (c == '\0') {
                c = EOF;
            } else {
                parser->string_source_index++;
            }
        } else {
            c = fgetc(parser->file);
        }

        if (length == parser->line_capacity) {
            parser->line_capacity *= 2;
            parser->line = xrealloc(parser->line, parser->line_capacity);
        }

        switch (c) {
        /* terminate line at \n or EOF */
        case EOF:
            if (length == 0) {
                if (parser->number_of_pushed_files == 0) {
                    return false;
                }
                /* pop the file */
                fclose(parser->file);
                parser->number_of_pushed_files--;
                parser->label =
                    parser->file_stack[parser->number_of_pushed_files].label;
                parser->file =
                    parser->file_stack[parser->number_of_pushed_files].file;
                break;
            }
        /* fall through */
        case '\n':
            parser->column = 0;
            parser->line[length] = '\0';
            return true;

        default:
            parser->line[length++] = c;
        }
    }
    return false;
}

/* Skip over empty characters (space). */
static void skip_space(Parser *parser)
{
    while (isspace(parser->line[parser->column])) {
        parser->column++;
    }
}

/* Skip leading space and put the next character into @parser->character. */
static parser_error_t parse_character(Parser *parser)
{
    skip_space(parser);

    parser->item_start_column = parser->column;
    if (parser->line[parser->column] == '\0') {
        return PARSER_ERROR_UNEXPECTED;
    }
    parser->character = parser->line[parser->column++];
    return PARSER_SUCCESS;
}

/* Skip leading space and load the next identifier into @parser->indentifier. */
static parser_error_t parse_identifier(Parser *parser)
{
    size_t length;

    skip_space(parser);

    parser->item_start_column = parser->column;

    length = 0;
    /* identifiers are quite flexible, they may even start with a number, any
     * chars of [a-zA-Z0-9-] are allowed
     */
    while (isalnum(parser->line[parser->column]) ||
            parser->line[parser->column] == '-') {
        length++;
        if (length == sizeof(parser->identifier)) {
            return PARSER_ERROR_TOO_LONG;
        }
        parser->column++;
    }

    if (length == 0) {
        return PARSER_UNEXPECTED;
    }

    memcpy(parser->identifier, &parser->line[parser->item_start_column], length);
    parser->identifier[length] = '\0';
    return PARSER_SUCCESS;
}

/* Parse a boolean with some tolerance on the wording. */
static parser_error_t parse_boolean(Parser *parser)
{
    static const char *truth_values[] = {
        "on", "true", "yes", "1"
    };
    static const char *false_values[] = {
        "off", "false", "no", "0"
    };

    parser_error_t error;

    skip_space(parser);

    error = parse_identifier(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    for (uint32_t i = 0; i < SIZE(truth_values); i++) {
        if (strcasecmp(truth_values[i], parser->identifier) == 0) {
            parser->data.boolean = true;
            return PARSER_SUCCESS;
        }
    }

    for (uint32_t i = 0; i < SIZE(false_values); i++) {
        if (strcasecmp(false_values[i], parser->identifier) == 0) {
            parser->data.boolean = false;
            return PARSER_SUCCESS;
        }
    }

    return PARSER_ERROR_INVALID_BOOLEAN;
}

/* Parse any text that may include escaped characters.
 *
 * Note that this stops at a semicolon.
 */
static parser_error_t parse_string(Parser *parser)
{
    size_t end;
    char byte;
    utf8_t *string;
    size_t index = 0;
    size_t last_index = 0;

    skip_space(parser);

    parser->item_start_column = parser->column;

    end = parser->column;
    /* read until the end of line or the first semicolon that is not escaped */
    for (; byte = parser->line[end], byte != '\0' && byte != ';'; end++) {
        /* check if the backslash it not at the end of the line */
        if (byte == '\\') {
            end++;
            if (parser->line[end] == '\0') {
                /* indicate the point of failure */
                parser->item_start_column = end - 1;
                parser->column = end - 1;
                return PARSER_ERROR_TRAILING_BACKSLASH;
            }
        }
    }

    if (parser->column == end) {
        /* there was absolutely nothing there */
        return PARSER_UNEXPECTED;
    }

    /* allocate enough bytes to hold on to all characters and the null
     * terminator, in reality (if characters are escaped) this may be less but
     * never more
     */
    string = xmalloc(end - parser->column + 1);

    end = parser->column;
    /* a second pass to handle escaped characters */
    for (; byte = parser->line[end], byte != '\0' && byte != ';'; end++) {
        if (byte == '\\') {
            end++;
            switch (parser->line[end]) {
            /* handle some standard escape sequences */
            case 'a': byte = '\a'; break;
            case 'b': byte = '\b'; break;
            case 'e': byte = '\x1b'; break;
            case 'f': byte = '\f'; break;
            case 'n': byte = '\n'; break;
            case 'r': byte = '\r'; break;
            case 't': byte = '\t'; break;
            case 'v': byte = '\v'; break;
            default:
                byte = parser->line[end];
            }
            last_index = index;
        } else if (!isspace(byte)) {
            last_index = index;
        }
        string[index] = byte;
        index++;
    }

    /* null terminate the string */
    string[last_index + 1] = '\0';

    /* trim anything we allocated too much */
    RESIZE(string, last_index + 2);

    parser->data.string = string;

    parser->column = end;
    return PARSER_SUCCESS;
}

/* Parse an integer in simple decimal notation. */
static parser_error_t parse_integer(Parser *parser)
{
    int32_t sign = 1;
    int32_t integer;

    skip_space(parser);

    parser->item_start_column = parser->column;

    /* get a preceding sign if any */
    if (parser->line[parser->column] == '+') {
        parser->column++;
    } else if (parser->line[parser->column] == '-') {
        sign = -1;
        parser->column++;
    } else if (!isdigit(parser->line[parser->column])) {
        return PARSER_UNEXPECTED;
    }

    /* need digits now */
    if (!isdigit(parser->line[parser->column])) {
        return PARSER_ERROR_UNEXPECTED;
    }

    /* read all digits while not overflowing */
    for (integer = 0; isdigit(parser->line[parser->column]); parser->column++) {
        integer *= 10;
        integer += parser->line[parser->column] - '0';
        if (integer >= PARSER_INTEGER_LIMIT) {
            integer = PARSER_INTEGER_LIMIT;
        }
    }

    parser->data.integer = sign * integer;

    return PARSER_SUCCESS;
}

/* Parse a set of up to 4 integers. */
static parser_error_t parse_quad(Parser *parser)
{
    int32_t quad[4];
    parser_error_t error;
    uint32_t i = 0;

    for (i = 0; i < SIZE(parser->data.quad); i++) {
        skip_space(parser);
        /* allow a premature end: not enough integers */
        if (parser->line[parser->column] == '\0') {
            break;
        }
        error = parse_integer(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }
        quad[i] = parser->data.integer;
    }

    switch (i) {
    /* if one value is specified, duplicate it to all other values */
    case 1:
        quad[1] = quad[0];
        quad[2] = quad[0];
        quad[3] = quad[0];
        break;

    /* if two values are specified, duplicate it to the other two values */
    case 2:
        quad[2] = quad[0];
        quad[3] = quad[1];
        break;

    /* no meaningful interpretations for other cases */
    default:
        return PARSER_ERROR_INVALID_QUAD;
    }

    memcpy(parser->data.quad, quad, sizeof(quad));
    return PARSER_SUCCESS;
}

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static parser_error_t parse_color(Parser *parser)
{
    uint32_t color;
    uint32_t count;

    if (parse_character(parser) != PARSER_SUCCESS || parser->character != '#') {
        return PARSER_UNEXPECTED;
    }

    for (color = 0, count = 0; isxdigit(parser->line[parser->column]);
            count++, parser->column++) {
        color <<= 4;
        color += isdigit(parser->line[parser->column]) ?
            parser->line[parser->column] - '0' :
            tolower(parser->line[parser->column]) + 0xa - 'a';
    }

    if (count != 6) {
        return PARSER_ERROR_BAD_COLOR_FORMAT;
    }

    parser->data.color = color;

    return PARSER_SUCCESS;
}

/* Parse key modifiers, e.g.: `Control+Shift`. */
static parser_error_t parse_modifiers(Parser *parser)
{
    parser_error_t error;
    uint16_t modifier;

    parser->data.modifiers = 0;
    while (error = parse_identifier(parser), error == PARSER_SUCCESS) {
        modifier = translate_string_to_modifier(parser->identifier);
        if (modifier == INVALID_MODIFIER) {
            error = PARSER_ERROR_INVALID_MODIFIERS;
            break;
        }

        parser->data.modifiers |= modifier;

        /* go to the next '+' */
        if (parse_character(parser) != PARSER_SUCCESS) {
            break;
        }
        if (parser->character != '+') {
            error = PARSER_ERROR_UNEXPECTED;
            break;
        }
    }
    return error;
}

/* Parse a cursor constant, e.g.: left-ptr. */
static parser_error_t parse_cursor(Parser *parser)
{
    parser_error_t error;
    core_cursor_t cursor;

    error = parse_identifier(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    /* translate - to _ */
    for (uint32_t i = 0; parser->identifier[i] != '\0'; i++) {
        if (parser->identifier[i] == '-') {
            parser->identifier[i] = '_';
        }
    }

    cursor = string_to_cursor(parser->identifier);
    if (cursor == XCURSOR_MAX) {
        return PARSER_ERROR_INVALID_CURSOR;
    }

    parser->data.cursor = cursor;

    return PARSER_SUCCESS;
}

/* Duplicates given @value deeply into itself. */
void duplicate_data_value(parser_data_type_t type,
        union parser_data_value *value)
{
    switch (type) {
    /* do a copy of the string */
    case PARSER_DATA_TYPE_STRING:
        value->string = (utf8_t*) xstrdup((char*) value->string);
        break;

    /* these have no data that needs to be deep copied */
    case PARSER_DATA_TYPE_BOOLEAN:
    case PARSER_DATA_TYPE_VOID:
    case PARSER_DATA_TYPE_INTEGER:
    case PARSER_DATA_TYPE_QUAD:
    case PARSER_DATA_TYPE_COLOR:
    case PARSER_DATA_TYPE_MODIFIERS:
    case PARSER_DATA_TYPE_CURSOR:
        break;
    }
}

/* Frees the resources the given data value occupies. */
void clear_data_value(parser_data_type_t type, union parser_data_value *value)
{
    switch (type) {
    /* free the string value */
    case PARSER_DATA_TYPE_STRING:
        free(value->string);
        break;

    /* these have no data that needs to be cleared */
    case PARSER_DATA_TYPE_BOOLEAN:
    case PARSER_DATA_TYPE_VOID:
    case PARSER_DATA_TYPE_INTEGER:
    case PARSER_DATA_TYPE_QUAD:
    case PARSER_DATA_TYPE_COLOR:
    case PARSER_DATA_TYPE_MODIFIERS:
    case PARSER_DATA_TYPE_CURSOR:
        break;
    }
}

/* Reads modifiers in the from modifier1+modifier2+... but stops at the last
 * identifier in the list, this be accessible in `parser->identifier`.
 */
static parser_error_t parse_button_or_key_modifiers(Parser *parser,
        uint16_t *modifiers)
{
    parser_error_t error;
    uint16_t modifier;

    *modifiers = 0;

    /* first, read the modifiers and key symbol */
    while (error = parse_identifier(parser), error == PARSER_SUCCESS) {
        /* try to find a next '+', if not found, then that must be a none
         * modifier
         */
        skip_space(parser);
        if (parser->line[parser->column] != '+') {
            break;
        }

        modifier = translate_string_to_modifier(parser->identifier);
        if (modifier == INVALID_MODIFIER) {
            error = PARSER_ERROR_INVALID_MODIFIERS;
            break;
        }

        *modifiers |= modifier;

        /* skip over '+' */
        parser->column++;
    }
    return error;
}

/* Parse binding flags, e.g.: `--release --transparent` */
static parser_error_t parse_binding_flags(Parser *parser, uint16_t *flags)
{
    parser_error_t error = PARSER_SUCCESS;

    *flags = 0;
    while (skip_space(parser), parser->line[parser->column] == '-') {
        error = parse_identifier(parser);
        if (error != PARSER_SUCCESS) {
            break;
        }
        if (strcasecmp(parser->identifier, "--release") == 0) {
            *flags |= BINDING_FLAG_RELEASE;
        } else if (strcasecmp(parser->identifier, "--transparent") == 0) {
            *flags |= BINDING_FLAG_TRANSPARENT;
        } else {
            error = PARSER_ERROR_INVALID_BUTTON_FLAG;
            break;
        }
    }
    return error;
}

/* Parse a semicolon separated list of actions. */
static parser_error_t parse_actions(Parser *parser,
        Action **destination_actions,
        uint32_t *destination_number_of_actions)
{
    parser_error_t error;

    Action *actions = NULL, *action;
    uint32_t number_of_actions = 0;

    while (error = parse_identifier(parser), error != PARSER_ERROR_TOO_LONG) {
        if (error != PARSER_SUCCESS) {
            error = PARSER_ERROR_MISSING_ACTION;
            break;
        }

        RESIZE(actions, number_of_actions + 1);
        action = &actions[number_of_actions];

        /* get the action name */
        action->code = string_to_action(parser->identifier);
        if (action->code == ACTION_NULL) {
            error = PARSER_ERROR_INVALID_ACTION;
            break;
        }
        memset(&action->parameter, 0, sizeof(action->parameter));

        /* get the action value */
        const parser_data_type_t data_type = get_action_data_type(action->code);
        if (data_type != PARSER_DATA_TYPE_VOID) {
            error = data_types[data_type].parse(parser);
            /* if the argument is optional, it is fine when an unexpected token
             * appears
             */
            if (error == PARSER_UNEXPECTED &&
                    has_action_optional_argument(action->code)) {
                error = PARSER_SUCCESS;
            } else {
                if (error != PARSER_SUCCESS) {
                    break;
                }
                action->parameter = parser->data;
            }
        }

        number_of_actions++;

        skip_space(parser);
        
        if (parser->line[parser->column] != ';') {
            break;
        }
        parser->column++;
    }

    if (error != PARSER_SUCCESS) {
        free_actions(actions, number_of_actions);
        return error;
    }

    *destination_actions = actions;
    *destination_number_of_actions = number_of_actions;
    return PARSER_SUCCESS;
}

/* Parse a mousebinding, e.g.: `Button2 close-window`. */
static parser_error_t parse_button(Parser *parser)
{
    parser_error_t error;

    error = parse_button_or_key_modifiers(parser, &parser->button.modifiers);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    parser->button.modifiers |= parser->configuration.mouse.modifiers;

    parser->button.index = translate_string_to_button(parser->identifier);
    if (parser->button.index == 0) {
        return PARSER_ERROR_INVALID_BUTTON;
    }

    error = parse_binding_flags(parser, &parser->button.flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_actions(parser, &parser->button.actions,
            &parser->button.number_of_actions);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    return PARSER_SUCCESS;
}

/* Parse a keybinding, e.g.: `Shift+v split-horizontally ; move-right`. */
static parser_error_t parse_key(Parser *parser)
{
    parser_error_t error;

    error = parse_button_or_key_modifiers(parser, &parser->key.modifiers);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    parser->key.modifiers |= parser->configuration.keyboard.modifiers;

    /* intepret starting with a digit as keycode */
    if (isdigit(parser->identifier[0])) {
        parser->key.key_symbol = XCB_NONE;
        parser->key.key_code = 0;
        for (uint32_t i = 0; parser->identifier[i] != '\0'; i++) {
            if (!isdigit(parser->identifier[i])) {
                error = PARSER_ERROR_INVALID_KEY_SYMBOL;
                break;
            }
            parser->key.key_code *= 10;
            parser->key.key_code += parser->identifier[i] - '0';
        }
    } else {
        parser->key.key_symbol = string_to_keysym(parser->identifier);
        if (parser->key.key_symbol == XCB_NONE) {
            error = PARSER_ERROR_INVALID_KEY_SYMBOL;
        }
        parser->key.key_code = 0;
    }

    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_binding_flags(parser, &parser->key.flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_actions(parser, &parser->key.actions,
            &parser->key.number_of_actions);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    return PARSER_SUCCESS;
}

/* Merges the default keybindings into the current parser keybindings. */
static parser_error_t merge_default_mouse(Parser *parser)
{
    merge_with_default_button_bindings(&parser->configuration);
    return PARSER_SUCCESS;
}

/* Merges the default keybindings into the current parser keybindings. */
static parser_error_t merge_default_keyboard(Parser *parser)
{
    merge_with_default_key_bindings(&parser->configuration);
    return PARSER_SUCCESS;
}

/* Parse a list of start up actions. */
static parser_error_t parse_startup_actions(Parser *parser)
{
    parser_error_t error;
    Action *actions;
    uint32_t number_of_actions;

    error = parse_actions(parser, &actions, &number_of_actions);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    /* append the parsed actions to the startup actions */
    RESIZE(parser->configuration.startup.actions,
            parser->configuration.startup.number_of_actions +
                number_of_actions);
    memcpy(&parser->configuration.startup.actions[
                parser->configuration.startup.number_of_actions],
            actions,
            sizeof(*actions) * number_of_actions);
    parser->configuration.startup.number_of_actions += number_of_actions;
    free(actions);
    return PARSER_SUCCESS;
}

/* Parse a binding for the mouse. */
static parser_error_t parse_mouse_binding(Parser *parser)
{
    parser_error_t error;
    struct configuration_button *button;

    error = parse_button(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    button = find_configured_button(&parser->configuration,
            parser->button.modifiers, parser->button.index,
            parser->button.flags);

    if (button != NULL) {
        free_actions(button->actions, button->number_of_actions);
    } else {
        RESIZE(parser->configuration.mouse.buttons,
                parser->configuration.mouse.number_of_buttons + 1);
        button = &parser->configuration.mouse.buttons[
            parser->configuration.mouse.number_of_buttons];
        parser->configuration.mouse.number_of_buttons++;
    }
    *button = parser->button;
    return PARSER_SUCCESS;
}

/* Parse a binding for the keyboard. */
static parser_error_t parse_keyboard_binding(Parser *parser)
{
    parser_error_t error;
    struct configuration_key *key;

    error = parse_key(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    if (parser->key.key_symbol == XCB_NONE) {
        key = find_configured_key_by_code(&parser->configuration,
                parser->key.modifiers, parser->key.key_code,
                parser->key.flags);
    } else {
        key = find_configured_key_by_symbol(&parser->configuration,
                parser->key.modifiers, parser->key.key_symbol,
                parser->key.flags);
    }

    if (key != NULL) {
        free_actions(key->actions, key->number_of_actions);
    } else {
        RESIZE(parser->configuration.keyboard.keys,
                parser->configuration.keyboard.number_of_keys + 1);
        key = &parser->configuration.keyboard.keys[
            parser->configuration.keyboard.number_of_keys];
        parser->configuration.keyboard.number_of_keys++;
    }
    *key = parser->key;
    return PARSER_SUCCESS;
}

/* Parse an association. */
static parser_error_t parse_assignment_association(Parser *parser)
{
    parser_error_t error;
    struct configuration_association association;

    error = parse_integer(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    if (parser->data.integer < 0) {
        return PARSER_ERROR_EXPECTED_UNSIGNED_INTEGER;
    }
    association.number = parser->data.integer;

    error = parse_string(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    association.instance_pattern = parser->data.string;

    if (parser->line[parser->column] != ';') {
        free(association.instance_pattern);
        return PARSER_ERROR_EXPECTED_SEPARATOR;
    }

    /* skip over ';' */
    parser->column++;

    error = parse_string(parser);
    if (error != PARSER_SUCCESS) {
        free(association.instance_pattern);
        return error;
    }
    association.class_pattern = parser->data.string;

    /* an optional list of actions may be supplied */
    if (parser->line[parser->column] == ';') {
        parser->column++;
        error = parse_actions(parser, &association.actions,
                &association.number_of_actions);
    }

    if (error != PARSER_SUCCESS) {
        free(association.instance_pattern);
        free(association.class_pattern);
        return error;
    }

    RESIZE(parser->configuration.assignment.associations,
            parser->configuration.assignment.number_of_associations + 1);
    parser->configuration.assignment.associations[
        parser->configuration.assignment.number_of_associations] = association;
    parser->configuration.assignment.number_of_associations++;

    return PARSER_SUCCESS;
}

/* Parses and handles given textual line. */
parser_error_t parse_line(Parser *parser)
{
    parser_error_t error;

    /* remove leading whitespace */
    skip_space(parser);
    
    /* ignore empty lines and comments */
    if (parser->line[parser->column] == '\0' || parser->line[parser->column] == '#') {
        parser->column += strlen(&parser->line[parser->column]);
        return PARSER_SUCCESS;
    }

    if (parser->line[parser->column] == '[') {
        parser->column++;

        error = parse_identifier(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }

        /* check if the label exists */
        for (parser_label_t i = 0; i < PARSER_LABEL_MAX; i++) {
            if (strcasecmp(labels[i].name, parser->identifier) == 0) {
                parser->label = i;
                /* check for an ending ']' */
                error = parse_character(parser);
                if (error != PARSER_SUCCESS || parser->character != ']') {
                    return PARSER_ERROR_MISSING_CLOSING;
                }
                parser->has_label[i] = true;
                return PARSER_SUCCESS;
            }
        }
        return PARSER_ERROR_INVALID_LABEL;
    }

    /* if this is the end of the string already, then there is no hope */
    if (parser->line[parser->column] == '\0') {
        return PARSER_ERROR_PREMATURE_LINE_END;
    }

    /* get the variable/command name */
    error = parse_identifier(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    /* check for a general parser command */
    if (strcasecmp(parser->identifier, "include") == 0) {
        char *path;

        /* check for a stack overflow */
        if (parser->number_of_pushed_files == SIZE(parser->file_stack)) {
            return PARSER_ERROR_INCLUDE_OVERFLOW;
        }

        /* push the current label and file onto the file stack */
        parser->file_stack[parser->number_of_pushed_files].label =
            parser->label;
        parser->file_stack[parser->number_of_pushed_files].file = parser->file;
        parser->number_of_pushed_files++;

        /* get the file name */
        error = parse_string(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }

        /* expand the file path */
        if (parser->data.string[0] == '~' && parser->data.string[1] == '/') {
            path = xasprintf("%s/%s", fensterchef_home,
                    &parser->data.string[2]);
            free(parser->data.string);
        } else {
            path = (char*) parser->data.string;
        }

        /* open the file */
        parser->file = fopen(path, "r");
        free(path);
        if (parser->file == NULL) {
            return PARSER_ERROR_INVALID_INCLUDE;
        }

        /* reset the label */
        parser->label = 0;

        return PARSER_SUCCESS;
    }

    /* check for a variable setting */
    for (uint32_t i = 0; i < SIZE(labels[parser->label].variables); i++) {
        const struct configuration_parser_label_variable *const variable =
            &labels[parser->label].variables[i];
        if (variable->name == NULL) {
            break;
        }

        if (strcasecmp(variable->name, parser->identifier) == 0) {
            error = data_types[variable->data_type].parse(parser);
            if (error != PARSER_SUCCESS) {
                return error;
            }

            /* set the struct member at given offset */
            union parser_data_value *const value = (union parser_data_value*)
                ((uint8_t*) &parser->configuration + variable->offset);
            clear_data_value(variable->data_type, value);
            memcpy(value, &parser->data, data_types[variable->data_type].size);
            return PARSER_SUCCESS;
        }
    }

    /* check for a parser command */
    for (uint32_t i = 0; i < SIZE(commands[parser->label]); i++) {
        const struct configuration_parser_command *const command =
            &commands[parser->label][i];
        if (command->name == NULL) {
            break;
        }

        if (strcmp(command->name, parser->identifier) == 0) {
            return command->procedure(parser);
        }
    }

    /* rewind before the identifier */
    parser->column = parser->item_start_column;

    /* check if the label has a special parser */
    if (labels[parser->label].special_parser == NULL) {
        return PARSER_ERROR_INVALID_VARIABLE_NAME;
    }

    return labels[parser->label].special_parser(parser);
}
