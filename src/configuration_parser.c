#include <ctype.h>
#include <string.h>

#include <X11/keysym.h>
#include <X11/Xlib.h> // XStringToKeysym

#include "configuration_parser.h"
#include "log.h"
#include "utility.h"

/* conversion from parser error to string */
static const char *parser_error_strings[] = {
    [PARSER_SUCCESS] = "success",

    [PARSER_ERROR_TRAILING] = "trailing characters",
    [PARSER_ERROR_TOO_LONG] = "identifier exceeds character limit "
        STRINGIFY(PARSER_IDENTIFIER_LIMIT),
    [PARSER_ERROR_INVALID_LABEL] = "invalid label name",
    [PARSER_ERROR_MISSING_CLOSING] = "missing a closing ']'",
    [PARSER_ERROR_NOT_IN_LABEL] = "not in a label yet, use `[<label>]` on a previous line",
    [PARSER_ERROR_INVALID_BOOLEAN] = "invalid boolean value",
    [PARSER_ERROR_INVALID_VARIABLE_NAME] = "the current label does not have that variable name",
    [PARSER_ERROR_BAD_COLOR_FORMAT] = "bad color format (expect #XXXXXX)",
    [PARSER_ERROR_PREMATURE_LINE_END] = "premature line end",
    [PARSER_ERROR_INVALID_MODIFIERS] = "invalid modifiers",
    [PARSER_ERROR_INVALID_KEY_SYMBOL] = "invalid key symbol name",
    [PARSER_ERROR_MISSING_ACTION] = "action value is missing",
    [PARSER_ERROR_INVALID_ACTION] = "invalid action value",
    [PARSER_ERROR_UNEXPECTED] = "unexpected tokens"
};

/* conversion of string to modifier mask */
static const struct modifier_string {
    const char *name;
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
    }
};

/* Converts @error to a string. */
const char *parser_string_error(parser_error_t error)
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
        c = fgetc(parser->file);

        if (length == parser->line_capacity) {
            parser->line_capacity *= 2;
            parser->line = xrealloc(parser->line, parser->line_capacity);
        }

        switch (c) {
        /* terminate line at \n or EOF */
        case EOF:
            if (length == 0) {
                return false;
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
    while (isalnum(parser->line[parser->column]) ||
            parser->line[parser->column] == '-') {
        length++;
        if (length == sizeof(parser->identifier)) {
            return PARSER_ERROR_TOO_LONG;
        }
        parser->column++;
    }

    if (length == 0) {
        return PARSER_ERROR_UNEXPECTED;
    }

    memcpy(parser->identifier, &parser->line[parser->item_start_column], length);
    parser->identifier[length] = '\0';
    return PARSER_SUCCESS;
}

/* Parse a boolean with some tolerance on the wording. */
static parser_error_t parse_boolean(Parser *parser)
{
    static const char *truth_values[] = {
        "on", "true", "yes"
    };
    static const char *false_values[] = {
        "off", "false", "no"
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

/* Parse any text without leading or trailing space.
 *
 * This stops at a semicolon!
 */
static parser_error_t parse_string(Parser *parser)
{
    size_t end, real_end;

    skip_space(parser);

    end = parser->column;
    real_end = end;
    while (parser->line[end] != '\0' && parser->line[end] != ';') {
        if (!isspace(parser->line[end])) {
            real_end = end + 1;
        }
        end++;
    }

    parser->data.string = (uint8_t*) xstrndup(&parser->line[parser->column],
            real_end - parser->column);
    parser->column = end;
    return PARSER_SUCCESS;
}

/* Parse an integer in simple decimal notation. */
static parser_error_t parse_integer(Parser *parser)
{
    int32_t sign;
    int32_t integer;

    skip_space(parser);

    parser->item_start_column = parser->column;

    if (parser->line[parser->column] == '+') {
        sign = 1;
        parser->column++;
    } else if (parser->line[parser->column] == '-') {
        sign = -1;
        parser->column++;
    } else {
        sign = 1;
    }

    if (!isdigit(parser->line[parser->column])) {
        return PARSER_ERROR_UNEXPECTED;
    }

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

    memset(quad, 0, sizeof(quad));
    for (uint32_t i = 0; i < SIZE(parser->data.quad); i++) {
        skip_space(parser);
        if (parser->line[parser->column] == '\0') {
            break;
        }
        error = parse_integer(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }
        quad[i] = parser->data.integer;
    }
    memcpy(parser->data.quad, quad, sizeof(quad));
    return PARSER_SUCCESS;
}

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static parser_error_t parse_color(Parser *parser)
{
    uint32_t color;
    uint32_t count;

    if (parse_character(parser) != PARSER_SUCCESS) {
        return PARSER_ERROR_PREMATURE_LINE_END;
    }

    if (parser->character != '#') {
        return PARSER_ERROR_BAD_COLOR_FORMAT;
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

/* Parse key modifiers, e.g.: Control+Shift. */
static parser_error_t parse_modifiers(Parser *parser)
{
    parser_error_t error;
    uint32_t string_index;

    parser->data.modifiers = 0;
    error = parse_identifier(parser);
    while (error == PARSER_SUCCESS) {
        /* find the modifier */
        string_index = (uint32_t) -1;
        for (uint32_t i = 0; i < SIZE(modifier_strings); i++) {
            if (strcasecmp(modifier_strings[i].name, parser->identifier) == 0) {
                string_index = i;
                break;
            }
        }
        if (string_index == (uint32_t) -1) {
            return PARSER_ERROR_INVALID_MODIFIERS;
        }

        parser->data.modifiers |= modifier_strings[string_index].modifier;

        /* go to the next '+' */
        if (parse_character(parser) != PARSER_SUCCESS) {
            return PARSER_SUCCESS;
        }
        if (parser->character != '+') {
            return PARSER_ERROR_UNEXPECTED;
        }

        error = parse_identifier(parser);
    }
    return error;
}

/* Duplicates given @value deeply into itself. */
void duplicate_data_value(parser_data_type_t type,
        union parser_data_value *value)
{
    switch (type) {
    /* do a copy of the string */
    case PARSER_DATA_TYPE_STRING:
        value->string = (uint8_t*) xstrdup((char*) value->string);
        break;

    /* these have no data that needs to be deep copied */
    case PARSER_DATA_TYPE_BOOLEAN:
    case PARSER_DATA_TYPE_VOID:
    case PARSER_DATA_TYPE_INTEGER:
    case PARSER_DATA_TYPE_QUAD:
    case PARSER_DATA_TYPE_COLOR:
    case PARSER_DATA_TYPE_MODIFIERS:
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
        break;
    }
}

/* Parse a keybinding, e.g.: "Shift+v split-horizontally ; move-right".
 *
 * This function assumes that an identifier was loaded into @parser.
 */
static parser_error_t parse_key(Parser *parser)
{
    parser_error_t error;
    uint32_t string_index;
    Action *action;

    parser->key.modifiers = parser->configuration->keyboard.modifiers;

    /* first, read the modifiers and key symbol */
    error = PARSER_SUCCESS;
    while (error == PARSER_SUCCESS) {
        /* try to find a next '+', if not found, then that must be a none
         * modifier
         */
        skip_space(parser);
        if (parser->line[parser->column] != '+') {
            parser->key.key_symbol = XStringToKeysym(parser->identifier);
            if (parser->key.key_symbol == NoSymbol) {
                return PARSER_ERROR_INVALID_KEY_SYMBOL;
            }
            break;
        }

        /* find the modifier */
        string_index = (uint32_t) -1;
        for (uint32_t i = 0; i < SIZE(modifier_strings); i++) {
            if (strcasecmp(modifier_strings[i].name, parser->identifier) == 0) {
                string_index = i;
                break;
            }
        }
        if (string_index == (uint32_t) -1) {
            return PARSER_ERROR_INVALID_MODIFIERS;
        }

        parser->key.modifiers |= modifier_strings[string_index].modifier;

        /* skip over '+' */
        parser->column++;

        error = parse_identifier(parser);
    }

    /* second, read the actions to perform */
    parser->key.actions = NULL;
    parser->key.number_of_actions = 0;
    while (true) {
        error = parse_identifier(parser);
        if (error == PARSER_ERROR_TOO_LONG) {
            free_key_actions(&parser->key);
            return error;
        }
        if (error != PARSER_SUCCESS) {
            free_key_actions(&parser->key);
            return PARSER_ERROR_MISSING_ACTION;
        }

        RESIZE(parser->key.actions, parser->key.number_of_actions + 1);
        action = &parser->key.actions[parser->key.number_of_actions];

        action->code = convert_string_to_action(parser->identifier);
        if (action->code == ACTION_NULL) {
            free_key_actions(&parser->key);
            return PARSER_ERROR_INVALID_ACTION;
        }
        memset(&action->parameter, 0, sizeof(action->parameter));

        const parser_data_type_t data_type = get_action_data_type(action->code);
        if (data_type != PARSER_DATA_TYPE_VOID) {
            error = data_types[data_type].parse(parser);
            if (error != PARSER_SUCCESS) {
                return error;
            }
            action->parameter = parser->data;
        }

        parser->key.number_of_actions++;

        skip_space(parser);
        
        if (parser->line[parser->column] != ';') {
            break;
        }
        parser->column++;
    }

    return PARSER_SUCCESS;
}

/* Merges the default keybindings into the current parser keybindings. */
static parser_error_t merge_default_keyboard(Parser *parser)
{
    merge_with_default_key_bindings(parser->configuration);
    return PARSER_SUCCESS;
}

/* Parses and handles given textual line. */
parser_error_t parse_line(Parser *parser)
{
    /* variables in the form <name> <value> */
    static const struct label {
        const char *name;
        struct label_variable {
            /* name of the variable */
            const char *name;
            /* type of the variable */
            parser_data_type_t data_type;
            /* offset within a `struct configuration` */
            size_t offset;
        } variables[8];
    } labels[PARSER_LABEL_MAX] = {
        [PARSER_LABEL_GENERAL] = {
            "general", {
            { "unused", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, general.unused) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_TILING] = {
            "tiling", {
            { "auto-fill-void", PARSER_DATA_TYPE_BOOLEAN,
                offsetof(struct configuration, tiling.auto_fill_void) },
            { "auto-remove-void", PARSER_DATA_TYPE_BOOLEAN,
                offsetof(struct configuration, tiling.auto_remove_void) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_FONT] = {
            "font", {
            { "name", PARSER_DATA_TYPE_STRING,
                offsetof(struct configuration, font.name) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_BORDER] = {
            "border", {
            { "size", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, border.size) },
            { "color", PARSER_DATA_TYPE_COLOR,
                offsetof(struct configuration, border.color) },
            { "focus-color", PARSER_DATA_TYPE_COLOR,
                offsetof(struct configuration, border.focus_color) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_GAPS] = {
            "gaps", {
            { "inner", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, gaps.inner) },
            { "outer", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, gaps.outer) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_NOTIFICATION] = {
            "notification", {
            { "duration", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, notification.duration) },
            { "padding", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, notification.padding) },
            { "border-color", PARSER_DATA_TYPE_COLOR,
                offsetof(struct configuration, notification.border_color) },
            { "border-size", PARSER_DATA_TYPE_INTEGER,
                offsetof(struct configuration, notification.border_size) },
            { "background", PARSER_DATA_TYPE_COLOR,
                offsetof(struct configuration, notification.background) },
            { "foreground", PARSER_DATA_TYPE_COLOR,
                offsetof(struct configuration, notification.foreground) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },

        [PARSER_LABEL_KEYBOARD] = {
            "keyboard", {
            { "modifiers", PARSER_DATA_TYPE_MODIFIERS,
                offsetof(struct configuration, keyboard.modifiers) },
            { "ignore-modifiers", PARSER_DATA_TYPE_MODIFIERS,
                offsetof(struct configuration, keyboard.ignore_modifiers) },
            /* null terminate the end */
            { NULL, 0, 0 } }
        },
    };

    /* the parser commands are in the form <command> <arguments> */
    static const struct command {
        /* the name of the command */
        const char *name;
        /* the procedure to execute (parses and executes the command) */
        parser_error_t (*procedure)(Parser *parser);
    } commands[PARSER_LABEL_MAX][2] = {
        [PARSER_LABEL_KEYBOARD] = {
            { "merge-default", merge_default_keyboard },
            /* null terminate the end */
            { NULL, NULL }
        }
    };

    parser_error_t error;

    struct configuration_key *key;

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
        for (parser_label_t i = PARSER_FIRST_LABEL; i < PARSER_LABEL_MAX; i++) {
            if (strcasecmp(labels[i].name, parser->identifier) == 0) {
                parser->label = i;
                /* check for an ending ']' */
                error = parse_character(parser);
                if (error != PARSER_SUCCESS || parser->character != ']') {
                    return PARSER_ERROR_MISSING_CLOSING;
                }
                return PARSER_SUCCESS;
            }
        }
        return PARSER_ERROR_INVALID_LABEL;
    }

    /* check if we are in a label */
    if (parser->label == PARSER_LABEL_NONE) {
        return PARSER_ERROR_NOT_IN_LABEL;
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

    /* check for a variable setting */
    for (uint32_t i = 0; i < SIZE(labels[parser->label].variables); i++) {
        const struct label_variable *const variable =
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
                ((uint8_t*) parser->configuration + variable->offset);
            clear_data_value(variable->data_type, value);
            memcpy(value, &parser->data, data_types[variable->data_type].size);
            return PARSER_SUCCESS;
        }
    }

    /* check for a parser command */
    for (uint32_t i = 0; i < SIZE(commands[parser->label]); i++) {
        const struct command *const command = &commands[parser->label][i];
        if (command->name == NULL) {
            break;
        }

        if (strcmp(command->name, parser->identifier) == 0) {
            return command->procedure(parser);
        }
    }

    /* special handling for defining keybindings */
    if (parser->label == PARSER_LABEL_KEYBOARD) {
        error = parse_key(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }

        key = find_configured_key(parser->configuration, parser->key.modifiers,
                parser->key.key_symbol);

        if (key != NULL) {
            free_key_actions(key);
        } else {
            RESIZE(parser->configuration->keyboard.keys,
                    parser->configuration->keyboard.number_of_keys + 1);
            key = &parser->configuration->keyboard.keys[
                parser->configuration->keyboard.number_of_keys++];
        }
        *key = parser->key;
        return PARSER_SUCCESS;
    }

    return PARSER_ERROR_INVALID_VARIABLE_NAME;
}
