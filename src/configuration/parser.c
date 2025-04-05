#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "configuration/parser.h"
#include "configuration/string_conversion.h"
#include "cursor.h"
#include "fensterchef.h"
#include "log.h"
#include "utility.h"
#include "x11_management.h"

/* conversion from parser error to string */
static const char *parser_error_strings[] = {
#define X(error, string) [error] = string,
    DEFINE_ALL_CONFIGURATION_PARSER_ERRORS
#undef X
};

/* Converts @error to a string. */
inline const char *parser_error_to_string(parser_error_t error)
{
    return parser_error_strings[error];
}

/* Prepare a parser for parsing. */
int initialize_parser(Parser *parser, const char *string, bool is_string_file)
{
    memset(parser, 0, sizeof(*parser));

    /* either load from a file or a string source */
    if (is_string_file) {
        parser->file = fopen(string, "r");
        if (parser->file == NULL) {
            return ERROR;
        }
    } else {
        parser->string_source = string;
    }

    parser->line_capacity = 128;
    parser->line = xmalloc(parser->line_capacity);

    parser->instruction_capacity = 4;
    RESIZE(parser->instructions, parser->instruction_capacity);
    return OK;
}

/* Free the resources the parser occupies.  */
void deinitialize_parser(Parser *parser)
{
    struct parser_file_stack_entry *entry;

    if (parser->file != NULL) {
        fclose(parser->file);
    }

    free(parser->line);

    for (unsigned i = 0; i < parser->number_of_pushed_files; i++) {
        entry = &parser->file_stack[i];
        free(entry->name);
        fclose(entry->file);
    }

    free(parser->instructions);
    for (uint32_t i = 0; i < parser->number_of_locals; i++) {
        free(parser->locals[i].name);
    }
    free(parser->locals);
}

#include "configuration/label_information.h"

/* Read the next line from the parsed files or string source into @parser->line.
 */
bool read_next_line(Parser *parser)
{
    size_t length;
    struct parser_file_stack_entry *entry;
    const char *new_line;
    const char *start;

    while (true) {
        parser->line_number++;

        /* if reading from a string source, simply find the line terminator (end
         * of string or new line)
         */
        if (parser->file == NULL) {
            start = &parser->string_source[parser->string_source_index];
            if (start[0] == '\0') {
                break;
            }

            new_line = strchr(start, '\n');
            if (new_line == NULL) {
                length = strlen(start);
                new_line = start + length;
            } else {
                length = new_line - start;
                parser->string_source_index++;
            }

            parser->string_source_index += length;

            if (parser->line_capacity <= length) {
                parser->line_capacity += length + 1;
                RESIZE(parser->line, parser->line_capacity);
            }

            memcpy(parser->line, start, length);
            parser->line[length] = '\0';
        } else {
            /* read an initial line segment, this might be the complete line */
            if (fgets(parser->line, parser->line_capacity,
                        parser->file) == NULL) {
                /* there is no more content in this file */

                if (parser->number_of_pushed_files == 0) {
                    /* no more lines */
                    break;
                }

                /* pop the file and equip the previous one */
                parser->number_of_pushed_files--;
                entry = &parser->file_stack[parser->number_of_pushed_files];
                free(entry->name);
                parser->file = entry->file;
                parser->label = entry->label;
                parser->line_number = entry->line_number;
                continue;
            }

            /* read the rest of the line by allocating more and more space */
            length = 0;
            do {
                length += strlen(&parser->line[length]);
                if (parser->line[length - 1] == '\n' || feof(parser->file)) {
                    break;
                }
                parser->line_capacity += length + 1;
                RESIZE(parser->line, parser->line_capacity);
            } while (fgets(&parser->line[length],
                        parser->line_capacity - length,
                        parser->file) != NULL);
        }

        /* check for a commented line */
        start = &parser->line[0];
        while (isspace(start[0])) {
            start++;
        }
        if (start[0] == '#') {
            continue;
        }

        /* remove trailing space */
        while (length > 0 && isspace(parser->line[length - 1])) {
            length--;
        }
        parser->line[length] = '\0';

        parser->column = 0;
        return true;
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
    size_t length = 0;

    skip_space(parser);

    /* identifiers are quite flexible, they may even start with a number, any
     * chars within the group [a-zA-Z0-9-] are allowed
     */
    while (isalnum(parser->line[parser->column]) ||
            parser->line[parser->column] == '-') {
        parser->identifier[length] = parser->line[parser->column];
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

    if (end == parser->column) {
        free(string);
        return PARSER_UNEXPECTED;
    }

    /* null terminate the string */
    string[last_index + 1] = '\0';

    /* trim anything we allocated too much */
    RESIZE(string, last_index + 2);

    *output = string;

    parser->column = end;
    return PARSER_SUCCESS;
}

/* Reads modifiers in the from Modifier1+Modifier2+... but stops at the last
 * identifier in the list, this be accessible in `parser->identifier`.
 */
static parser_error_t parse_button_or_key_modifiers(Parser *parser,
        unsigned *modifiers)
{
    parser_error_t error;
    unsigned modifier;
    unsigned old_start_column;

    *modifiers = 0;

    /* first, read the modifiers and key symbol */
    while (error = parse_identifier(parser), error == PARSER_SUCCESS) {
        /* save this value as `skip_space()` sets it to something else */
        old_start_column = parser->item_start_column;
        /* try to find a next '+', if not found, then that must be a none
         * modifier (the button index or key symbol)
         */
        skip_space(parser);
        if (parser->line[parser->column] != '+') {
            break;
        }

        if (string_to_modifier(parser->identifier, &modifier) != OK) {
            error = PARSER_ERROR_INVALID_MODIFIERS;
            parser->item_start_column = old_start_column;
            break;
        }

        *modifiers |= modifier;

        /* skip over '+' */
        parser->column++;
    }
    return error;
}

/* Parse binding flags, e.g.: `--release --transparent` */
static parser_error_t parse_binding_flags(Parser *parser, unsigned *flags)
{
    parser_error_t error = PARSER_SUCCESS;

    *flags = 0;
    while (skip_space(parser), parser->line[parser->column] == '-') {
        error = parse_identifier(parser);
        if (error != PARSER_SUCCESS) {
            break;
        }
        if (strcmp(parser->identifier, "--release") == 0) {
            *flags |= BINDING_FLAG_RELEASE;
        } else if (strcmp(parser->identifier, "--transparent") == 0) {
            *flags |= BINDING_FLAG_TRANSPARENT;
        } else {
            error = PARSER_ERROR_INVALID_BINDING_FLAG;
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

    if (string_to_button(parser->identifier, &button->index) != OK) {
        return PARSER_ERROR_INVALID_BUTTON;
    }

    error = parse_binding_flags(parser, &button->flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    reset_expression(parser);
    error = parse_expression_and_append(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    initialize_expression(&button->expression,
            parser->instructions, parser->instruction_size);

    return PARSER_SUCCESS;
}

/* Parse a key binding, e.g.: `Shift+v split-horizontally ; move-right`. */
static parser_error_t parse_key(Parser *parser, struct configuration_key *key)
{
    parser_error_t error;

    error = parse_button_or_key_modifiers(parser, &key->modifiers);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    key->modifiers |= parser->configuration.keyboard.modifiers;

    key->key_code = 0;
    /* intepret starting with a digit as keycode */
    if (isdigit(parser->identifier[0])) {
        int min_key_code, max_key_code;

        key->key_symbol = None;
        for (unsigned i = 0; parser->identifier[i] != '\0'; i++) {
            if (!isdigit(parser->identifier[i])) {
                error = PARSER_ERROR_INVALID_KEY_CODE;
                break;
            }
            key->key_code *= 10;
            key->key_code += parser->identifier[i] - '0';
            if (key->key_code > max_key_code) {
                break;
            }
        }

        XDisplayKeycodes(display, &min_key_code, &max_key_code);
        if (key->key_code < min_key_code || key->key_code > max_key_code) {
            error = PARSER_ERROR_INVALID_KEY_CODE;
        }
    } else {
        key->key_symbol = XStringToKeysym(parser->identifier);
        if (key->key_symbol == None) {
            error = PARSER_ERROR_INVALID_KEY_SYMBOL;
        }
    }

    if (error != PARSER_SUCCESS) {
        return error;
    }

    error = parse_binding_flags(parser, &key->flags);
    if (error != PARSER_SUCCESS) {
        return error;
    }

    reset_expression(parser);
    error = parse_expression_and_append(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    initialize_expression(&key->expression,
            parser->instructions, parser->instruction_size);

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

    reset_expression(parser);
    error = parse_expression_and_append(parser);
    if (error != PARSER_SUCCESS) {
        return error;
    }
    initialize_expression(&expression,
            parser->instructions, parser->instruction_size);

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

    if (key.key_symbol == None) {
        key_pointer = find_configured_key(&parser->configuration,
                key.modifiers, key.key_code, key.flags);
    } else {
        key_pointer = find_configured_key_by_key_symbol(&parser->configuration,
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
    parser_error_t error = PARSER_SUCCESS;
    int32_t number;
    struct configuration_association association = {
        .expression.instruction_size = 0
    };

    /* read the leading number */
    skip_space(parser);
    for (number = 0;
            isdigit(parser->line[parser->column]);
            parser->column++) {
        number *= 10;
        number += parser->line[parser->column] - '0';
        if (number > PARSER_INTEGER_LIMIT) {
            error = PARSER_ERROR_INTEGER_TOO_LARGE;
            break;
        }
    }
    if (error != PARSER_SUCCESS) {
        return error;
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
        reset_expression(parser);
        error = parse_expression_and_append(parser);
        if (error == PARSER_SUCCESS) {
            initialize_expression(&association.expression,
                    parser->instructions, parser->instruction_size);
        }
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
    struct parser_file_stack_entry *entry;

    /* remove leading whitespace */
    skip_space(parser);

    /* ignore empty lines */
    if (parser->line[parser->column] == '\0') {
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
            if (strcmp(labels[i].name, parser->identifier) == 0) {
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
    if (strcmp(parser->identifier, "include") == 0) {
        utf8_t *path;

        /* check for a stack overflow */
        if (parser->number_of_pushed_files == SIZE(parser->file_stack)) {
            return PARSER_ERROR_INCLUDE_OVERFLOW;
        }

        /* push the current file state onto the file stack */
        entry = &parser->file_stack[parser->number_of_pushed_files];
        entry->file = parser->file;
        entry->line_number = parser->line_number;
        entry->label = parser->label;

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
        parser->file = fopen(path, "r");
        if (parser->file == NULL) {
            free(path);
            return PARSER_ERROR_INVALID_INCLUDE;
        }

        entry->name = path;
        parser->number_of_pushed_files++;
        /* reset the label */
        parser->label = 0;
        return PARSER_SUCCESS;
    }

    /* check for a variable setting */
    for (unsigned i = 0; labels[parser->label].variables[i].name != NULL; i++) {
        const struct configuration_parser_label_variable *const variable =
            &labels[parser->label].variables[i];

        if (strcmp(variable->name, parser->identifier) == 0) {
            /* set the struct member at given offset */
            GenericData *const value = (GenericData*)
                ((uint8_t*) &parser->configuration + variable->offset);

            /* clear the old value */
            clear_data_value(variable->data_type, value);

            /* parse and move in the new value */
            switch (variable->data_type) {
            case DATA_TYPE_VOID:
                parser->instruction_size = 0;
                break;

            case DATA_TYPE_INTEGER: {
                reset_expression(parser);
                error = parse_expression_and_append(parser);
                if (error == PARSER_SUCCESS) {
                    const Expression expression = {
                        parser->instructions,
                        parser->instruction_size,
                    };
                    evaluate_expression(&expression, value);
                }
                break;
            }

            case DATA_TYPE_QUAD:
                reset_expression(parser);
                error = parse_quad_expression_and_append(parser);
                if (error == PARSER_SUCCESS) {
                    const Expression expression = {
                        parser->instructions,
                        parser->instruction_size,
                    };
                    evaluate_expression(&expression, value);
                }
                break;

            case DATA_TYPE_STRING:
                error = parse_string(parser, &value->string);
                break;

            /* not a real data type */
            case DATA_TYPE_MAX:
                break;
            }
            return error;
        }
    }

    /* check for a parser command */
    for (unsigned i = 0; i < SIZE(commands[parser->label]); i++) {
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

special_handling:
    /* check if the label has a special parser */
    if (labels[parser->label].special_parser == NULL) {
        return PARSER_ERROR_INVALID_VARIABLE_NAME;
    }

    return labels[parser->label].special_parser(parser);
}
