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
    { "none", 0 },

    { "shift", XCB_MOD_MASK_SHIFT },
    { "lock", XCB_MOD_MASK_LOCK },
    { "capslock", XCB_MOD_MASK_LOCK },
    { "ctrl", XCB_MOD_MASK_CONTROL },
    { "control", XCB_MOD_MASK_CONTROL },

    /* common synonyms for some modifiers */
    { "alt", XCB_MOD_MASK_1 },
    { "super", XCB_MOD_MASK_4 },

    { "mod1", XCB_MOD_MASK_1 },
    { "mod2", XCB_MOD_MASK_2 },
    { "mod3", XCB_MOD_MASK_3 },
    { "mod4", XCB_MOD_MASK_4 },
    { "mod5", XCB_MOD_MASK_5 }
};

/* conversion from string to button index */
static const struct button_string {
    /* the string representation of the button */
    const char *name;
    /* the button index */
    xcb_button_t button_index;
} button_strings[] = {
    /* buttons can also be Button<integer> to directly address the index */
    { "lbutton", 1 },
    { "leftbutton", 1 },

    { "mbutton", 2 },
    { "middlebutton", 2 },

    { "rbutton", 3 },
    { "rightbutton", 3 },

    { "scrollup", 4 },
    { "wheelup", 4 },

    { "scrolldown", 5 },
    { "wheeldown", 5 },

    { "scrollleft", 6 },
    { "wheelleft", 6 },

    { "scrollright", 7 },
    { "wheelright", 7 },

#define FIRST_X_BUTTON 8
#define NUMBER_OF_X_BUTTONS 247
    /* X buttons (extra buttons on the mouse usually) going from X1 (8) to
     * X247 (255), they have their own handling and are not listed here
     */
};

typedef parser_error_t (*parse_data_type_function_t)(Parser *parser,
        GenericData *output);

/* The void data type is just a placeholder, it expects nothing. */
static parser_error_t parse_void(Parser *parser, void *output)
{
    (void) parser;
    (void) output;
    return PARSER_SUCCESS;
}

/* Parse a boolean with some tolerance on the wording. */
static parser_error_t parse_boolean(Parser *parser, bool *output);

/* Parse a set of 4 integers. */
static parser_error_t parse_quad(Parser *parser, int32_t *output);

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static parser_error_t parse_color(Parser *parser, uint32_t *output);

/* Parse key modifiers, e.g.: Control+Shift. */
static parser_error_t parse_modifiers(Parser *parser, uint16_t *output);

/* Parse a cursor constant, e.g.: left-ptr. */
static parser_error_t parse_cursor(Parser *parser, core_cursor_t *output);

/* parser methods of all data types */
parse_data_type_function_t data_type_parsers[] = {
    [DATA_TYPE_VOID] = (parse_data_type_function_t) parse_void,
    [DATA_TYPE_BOOLEAN] = (parse_data_type_function_t) parse_boolean,
    [DATA_TYPE_STRING] = (parse_data_type_function_t) parse_string,
    [DATA_TYPE_INTEGER] = (parse_data_type_function_t) parse_integer,
    [DATA_TYPE_QUAD] = (parse_data_type_function_t) parse_quad,
    [DATA_TYPE_COLOR] = (parse_data_type_function_t) parse_color,
    [DATA_TYPE_MODIFIERS] = (parse_data_type_function_t) parse_modifiers,
    [DATA_TYPE_CURSOR] = (parse_data_type_function_t) parse_cursor,
};

#include "bits/configuration_parser_label_information.h"

/* Translate a string like "button1" to a button index. */
static xcb_button_t string_to_button(const char *string)
{
    /* parse indexes starting with "x" */
    if (string[0] == 'x') {
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
    } else if (string[0] == 'b' &&
            string[1] == 'u' &&
            string[2] == 't' &&
            string[3] == 't' &&
            string[4] == 'o' &&
            string[5] == 'n') {
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
        if (strcmp(button_strings[i].name, string) == 0) {
            return button_strings[i].button_index;
        }
    }
    return 0;
}

/* Translate a string like "shift" to a modifier bit. */
int string_to_modifier(const char *string, uint16_t *output)
{
    for (uint32_t i = 0; i < SIZE(modifier_strings); i++) {
        if (strcmp(modifier_strings[i].name, string) == 0) {
            *output = modifier_strings[i].modifier;
            return OK;
        }
    }
    return ERROR;
}

/* Translate a string like "false" to a boolean value. */
int string_to_boolean(const char *string, bool *output)
{
    static const char *truth_values[] = {
        "on", "true", "yes", "1"
    };
    static const char *false_values[] = {
        "off", "false", "no", "0"
    };

    for (uint32_t i = 0; i < SIZE(truth_values); i++) {
        if (strcmp(truth_values[i], string) == 0) {
            *output = true;
            return OK;
        }
    }

    for (uint32_t i = 0; i < SIZE(false_values); i++) {
        if (strcmp(false_values[i], string) == 0) {
            *output = false;
            return OK;
        }
    }
    return ERROR;
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
void skip_space(Parser *parser)
{
    while (isspace(parser->line[parser->column])) {
        parser->column++;
    }
    parser->item_start_column = parser->column;
}

/* Skip leading space and put the next character into @parser->character. */
parser_error_t parse_character(Parser *parser)
{
    skip_space(parser);

    if (parser->line[parser->column] == '\0') {
        return PARSER_ERROR_UNEXPECTED;
    }
    parser->character = parser->line[parser->column];
    parser->column++;
    return PARSER_SUCCESS;
}

/* Skip leading space and load the next identifier into @parser->indentifier. */
parser_error_t parse_identifier(Parser *parser)
{
    size_t length;

    skip_space(parser);

    length = 0;
    /* identifiers are quite flexible, they may even start with a number, any
     * chars of [a-zA-Z0-9-] are allowed
     */
    while (isalnum(parser->line[parser->column]) ||
            parser->line[parser->column] == '-') {
        parser->identifier[length] = parser->line[parser->column];
        parser->identifier_lower[length] =
            tolower(parser->line[parser->column]);
        length++;
        if (length == sizeof(parser->identifier)) {
            return PARSER_ERROR_TOO_LONG;
        }
        parser->column++;
    }

    if (length == 0) {
        return PARSER_UNEXPECTED;
    }

    parser->identifier[length] = '\0';
    parser->identifier_lower[length] = '\0';
    return PARSER_SUCCESS;
}

/* Parse a boolean with some tolerance on the wording. */
static parser_error_t parse_boolean(Parser *parser, bool *output)
{
    parser_error_t error;

    skip_space(parser);

    error = parse_identifier(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    if (string_to_boolean(parser->identifier_lower, output) != OK) {
        return PARSER_ERROR_INVALID_BOOLEAN;
    }
    return PARSER_SUCCESS;
}

/* Parse any text that may include escaped characters. */
parser_error_t parse_string(Parser *parser, utf8_t **output)
{
    size_t end;
    char byte;
    utf8_t *string;
    size_t index = 0;
    size_t last_index = 0;

    skip_space(parser);

    /* allocate enough bytes to hold on to all characters and the null
     * terminator, in reality this may be less but never more
     */
    string = xmalloc(strlen(parser->line) + 1);

    end = parser->column;
    for (; byte = parser->line[end], byte != '\0' && byte != ';' &&
            byte != '&' && byte != '|' && byte != ')'; end++) {
        if (byte == '\\') {
            end++;
            switch (parser->line[end]) {
            /* handle a trailing backslash */
            case '\0':
                byte = '\\';
                end--;
                break;

            /* handle some standard escape sequences */
            case 'a': byte = '\a'; break;
            case 'b': byte = '\b'; break;
            case 'e': byte = '\x1b'; break;
            case 'f': byte = '\f'; break;
            case 'n': byte = '\n'; break;
            case 'r': byte = '\r'; break;
            case 't': byte = '\t'; break;
            case 'v': byte = '\v'; break;
            case '\\': byte = '\\'; break;

            /* handle the escaping of special characters */
            case ';': byte = ';'; break;
            case '&': byte = '&'; break;
            case '|': byte = '|'; break;
            case ')': byte = ')'; break;

            /* simply ignore that there was a \ in the first place */
            default:
                string[index] = '\\';
                index++;
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

    *output = string;

    parser->column = end;
    return PARSER_SUCCESS;
}

/* Parse an integer in simple decimal notation. */
parser_error_t parse_integer(Parser *parser, int32_t *output)
{
    int32_t sign = 1;
    int32_t integer;

    skip_space(parser);

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

    *output = sign * integer;

    return PARSER_SUCCESS;
}

/* Parse a set of up to 4 integers. */
static parser_error_t parse_quad(Parser *parser, int32_t *output)
{
    int32_t quad[4];
    parser_error_t error;
    uint32_t i = 0;

    for (i = 0; i < SIZE(quad); i++) {
        skip_space(parser);
        /* allow a premature end: not enough integers */
        if (parser->line[parser->column] == '\0') {
            break;
        }
        error = parse_integer(parser, &quad[i]);
        if (error != PARSER_SUCCESS) {
            return error;
        }
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

    /* if four values are specified, nothing needs to be done */
    case 4:
        break;

    /* no meaningful interpretations for other cases */
    default:
        return PARSER_ERROR_INVALID_QUAD;
    }

    memcpy(output, quad, sizeof(quad));
    return PARSER_SUCCESS;
}

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static parser_error_t parse_color(Parser *parser, uint32_t *output)
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

    *output = color;

    return PARSER_SUCCESS;
}

/* Parse key modifiers, e.g.: `control+shift`. */
static parser_error_t parse_modifiers(Parser *parser, uint16_t *output)
{
    parser_error_t error;
    uint16_t modifier, modifiers = 0;

    while (error = parse_identifier(parser), error == PARSER_SUCCESS) {
        if (string_to_modifier(parser->identifier_lower, &modifier) != OK) {
            error = PARSER_ERROR_INVALID_MODIFIERS;
            break;
        }

        modifiers |= modifier;

        /* go to the next '+' */
        if (parse_character(parser) != PARSER_SUCCESS) {
            break;
        }
        if (parser->character != '+') {
            error = PARSER_ERROR_UNEXPECTED;
            break;
        }
    }

    *output = modifiers;
    return error;
}

/* Parse a cursor constant, e.g.: left-ptr. */
static parser_error_t parse_cursor(Parser *parser, core_cursor_t *output)
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

    *output = cursor;

    return PARSER_SUCCESS;
}

/* Reads modifiers in the from modifier1+modifier2+... but stops at the last
 * identifier in the list, this be accessible in `parser->identifier_lower`.
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
         * modifier (the button index or key symbol)
         */
        skip_space(parser);
        if (parser->line[parser->column] != '+') {
            break;
        }

        if (string_to_modifier(parser->identifier_lower, &modifier) != OK) {
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
        if (strcmp(parser->identifier_lower, "--release") == 0) {
            *flags |= BINDING_FLAG_RELEASE;
        } else if (strcmp(parser->identifier_lower, "--transparent") == 0) {
            *flags |= BINDING_FLAG_TRANSPARENT;
        } else {
            error = PARSER_ERROR_INVALID_BUTTON_FLAG;
            break;
        }
    }
    return error;
}

/* Parse a mouse button binding, e.g.: `button2 close-window`. */
static parser_error_t parse_button(Parser *parser,
        struct configuration_button *button)
{
    parser_error_t error;

    error = parse_button_or_key_modifiers(parser, &button->modifiers);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    button->modifiers |= parser->configuration.mouse.modifiers;

    button->index = string_to_button(parser->identifier_lower);
    if (button->index == 0) {
        return PARSER_ERROR_INVALID_BUTTON;
    }

    error = parse_binding_flags(parser, &button->flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_expression(parser, &button->expression);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    return PARSER_SUCCESS;
}

/* Parse a key binding, e.g.: `shift+v split-horizontally ; move-right`. */
static parser_error_t parse_key(Parser *parser, struct configuration_key *key)
{
    parser_error_t error;

    error = parse_button_or_key_modifiers(parser, &key->modifiers);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    key->modifiers |= parser->configuration.keyboard.modifiers;

    /* intepret starting with a digit as keycode */
    if (isdigit(parser->identifier_lower[0])) {
        key->key_symbol = XCB_NONE;
        key->key_code = 0;
        for (uint32_t i = 0; parser->identifier_lower[i] != '\0'; i++) {
            if (!isdigit(parser->identifier_lower[i])) {
                error = PARSER_ERROR_INVALID_KEY_SYMBOL;
                break;
            }
            key->key_code *= 10;
            key->key_code += parser->identifier_lower[i] - '0';
        }
    } else {
        key->key_symbol = string_to_keysym(parser->identifier);
        if (key->key_symbol == XCB_NONE) {
            error = PARSER_ERROR_INVALID_KEY_SYMBOL;
        }
        key->key_code = 0;
    }

    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_binding_flags(parser, &key->flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_expression(parser, &key->expression);
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
    Expression expression;

    error = parse_expression(parser, &expression);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    /* append the parsed expression to the startup expression */
    RESIZE(parser->configuration.startup.expression.instructions,
            parser->configuration.startup.expression.instruction_size +
                expression.instruction_size);
    memcpy(&parser->configuration.startup.expression.instructions[
                parser->configuration.startup.expression.instruction_size],
            expression.instructions,
            sizeof(*expression.instructions) * expression.instruction_size);
    parser->configuration.startup.expression.instruction_size +=
        expression.instruction_size;
    free(expression.instructions);
    return PARSER_SUCCESS;
}

/* Parse a binding for the mouse. */
static parser_error_t parse_mouse_binding(Parser *parser)
{
    parser_error_t error;
    struct configuration_button button, *button_pointer;

    error = parse_button(parser, &button);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    button_pointer = find_configured_button(&parser->configuration,
            button.modifiers, button.index, button.flags);

    if (button_pointer != NULL) {
        /* free the old button instructions */
        free(button_pointer->expression.instructions);
    } else {
        /* add the button to the end */
        RESIZE(parser->configuration.mouse.buttons,
                parser->configuration.mouse.number_of_buttons + 1);
        button_pointer = &parser->configuration.mouse.buttons[
            parser->configuration.mouse.number_of_buttons];
        parser->configuration.mouse.number_of_buttons++;
    }
    *button_pointer = button;
    return PARSER_SUCCESS;
}

/* Parse a binding for the keyboard. */
static parser_error_t parse_keyboard_binding(Parser *parser)
{
    parser_error_t error;
    struct configuration_key key, *key_pointer;

    error = parse_key(parser, &key);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    if (key.key_symbol == XCB_NONE) {
        key_pointer = find_configured_key_by_code(&parser->configuration,
                key.modifiers, key.key_code, key.flags);
    } else {
        key_pointer = find_configured_key_by_symbol(&parser->configuration,
                key.modifiers, key.key_symbol, key.flags);
    }

    if (key_pointer != NULL) {
        /* free the old now unused instructions */
        free(key_pointer->expression.instructions);
    } else {
        /* the key does not exist already, add a new one */
        RESIZE(parser->configuration.keyboard.keys,
                parser->configuration.keyboard.number_of_keys + 1);
        key_pointer = &parser->configuration.keyboard.keys[
            parser->configuration.keyboard.number_of_keys];
        parser->configuration.keyboard.number_of_keys++;
    }
    *key_pointer = key;
    return PARSER_SUCCESS;
}

/* Parse an association.
 *
 * Generally:
 * <number> <instance string> ; <class string> (; <expression>)?
 * 
 * Examples:
 * 12 * ; XTerm
 * 0 * ; firefox ; none
 */
static parser_error_t parse_assignment_association(Parser *parser)
{
    parser_error_t error;
    int32_t number;
    struct configuration_association association = {
        .expression.instruction_size = 0
    };

    /* read the leading number */
    error = parse_integer(parser, &number);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    if (number < 0) {
        return PARSER_ERROR_EXPECTED_UNSIGNED_INTEGER;
    }
    association.number = number;

    /* get the instance pattern */
    error = parse_string(parser, &association.instance_pattern);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    if (parser->line[parser->column] != ';') {
        free(association.instance_pattern);
        return PARSER_ERROR_EXPECTED_SEPARATOR;
    }

    /* skip over ';' */
    parser->column++;

    /* get the class pattern */
    error = parse_string(parser, &association.class_pattern);
    if (error != PARSER_SUCCESS) {
        free(association.instance_pattern);
        return error;
    }

    /* an optional expression may be supplied */
    if (parser->line[parser->column] == ';') {
        parser->column++;
        error = parse_expression(parser, &association.expression);
    }

    if (error != PARSER_SUCCESS) {
        free(association.instance_pattern);
        free(association.class_pattern);
        return error;
    }

    /* add the association to the end of the association list */
    RESIZE(parser->configuration.assignment.associations,
            parser->configuration.assignment.number_of_associations + 1);
    parser->configuration.assignment.associations[
        parser->configuration.assignment.number_of_associations] = association;
    parser->configuration.assignment.number_of_associations++;

    return PARSER_SUCCESS;
}

/* Parse the line within @parser. */
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
            if (strcmp(labels[i].name, parser->identifier_lower) == 0) {
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
    if (error == PARSER_UNEXPECTED) {
        /* jump to the bottom of the function to skip all the identifier checks
         */
        goto special_handling;
    }
    if (error != PARSER_SUCCESS) {
        return error;
    }

    /* check for a general parser command */
    if (strcmp(parser->identifier_lower, "include") == 0) {
        utf8_t *path;

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
        error = parse_string(parser, &path);
        if (error != PARSER_SUCCESS) {
            return error;
        }

        /* expand the file path */
        if (path[0] == '~' && path[1] == '/') {
            void *const free_me = path;
            path = (utf8_t*) xasprintf("%s/%s", Fensterchef_home, &path[2]);
            free(free_me);
        }

        /* open the file */
        parser->file = fopen((char*) path, "r");
        free(path);
        if (parser->file == NULL) {
            return PARSER_ERROR_INVALID_INCLUDE;
        }

        /* reset the label */
        parser->label = 0;

        return PARSER_SUCCESS;
    }

    /* check for a variable setting */
    for (uint32_t i = 0; labels[parser->label].variables[i].name != NULL; i++) {
        const struct configuration_parser_label_variable *const variable =
            &labels[parser->label].variables[i];

        if (strcmp(variable->name, parser->identifier_lower) == 0) {
            /* set the struct member at given offset */
            GenericData *const value = (GenericData*)
                ((uint8_t*) &parser->configuration + variable->offset);
            /* clear the old value */
            clear_data_value(variable->data_type, value);
            /* parse and move in the new value */
            error = data_type_parsers[variable->data_type](parser, value);
            if (error != PARSER_SUCCESS) {
                /* make sure no invalid pointers hang around because everything
                 * gets freed after failing to parse
                 */
                memset(value, 0, data_type_sizes[variable->data_type]);
                return error;
            }
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

        if (strcmp(command->name, parser->identifier_lower) == 0) {
            return command->procedure(parser);
        }
    }

    /* rewind before the identifier */
    parser->column = parser->item_start_column;

special_handling:
    /* check if the label has a special parser */
    if (labels[parser->label].special_parser == NULL) {
        return PARSER_ERROR_INVALID_VARIABLE_NAME;
    }

    return labels[parser->label].special_parser(parser);
}
