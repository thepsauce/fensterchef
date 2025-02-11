#include <ctype.h>
#include <string.h>

#include <X11/keysym.h>
#include <X11/Xlib.h> // XStringToKeysym

#include "configuration_parser.h"
#include "utility.h"

const struct modifier_string {
    const char *name;
    uint16_t modifier;
} modifier_translations[] = {
    { "Shift", XCB_MOD_MASK_SHIFT },
    { "Lock", XCB_MOD_MASK_LOCK },
    { "CapsLock", XCB_MOD_MASK_LOCK },
    { "Ctrl", XCB_MOD_MASK_CONTROL },
    { "Control", XCB_MOD_MASK_CONTROL },
    { "Alt", XCB_MOD_MASK_1 },
    { "Mod1", XCB_MOD_MASK_1 },
    { "Mod2", XCB_MOD_MASK_2 },
    { "Mod3", XCB_MOD_MASK_3 },
    { "Mod4", XCB_MOD_MASK_4 },
    { "Mod5", XCB_MOD_MASK_5 }
};

/* Read the next line from the file. */
bool read_next_line(struct parser_context *context)
{
    size_t length;

    length = 0;
    for (int c;; ) {
        c = fgetc(context->file);

        if (length == context->line_capacity) {
            context->line_capacity *= 2;
            context->line = xrealloc(context->line, context->line_capacity);
        }

        if (c == '\n' || c == EOF) {
            if (length > 0) {
                context->line[length] = '\0';
                return true;
            }
        } else {
            context->line[length++] = c;
        }
    
        if (c == EOF) {
            break;
        }
    }
    return false;
}

/* Parse any text without leading or trailing space. */
static int parse_name(char *string, struct parser_variable *variable)
{
    variable->type = PARSER_TYPE_NAME;
    variable->value.name = (FcChar8*) xstrdup(string);
    return 0;
}

/* Parse an integer in simple decimal notation. */
static int parse_integer(char *string, struct parser_variable *variable)
{
    uint32_t integer;

    for (integer = 0; isdigit(string[0]); ) {
        integer *= 10;
        integer += string[0] - '0';
    }

    while (isspace(string[0])) {
       string++;
    }

    if (string[0] != '\0') {
        return 1;
    }

    variable->value.integer = integer;

    return 0;
}

/* Parse a color in the format (X: hexadecimal digit): #XXXXXX. */
static int parse_color(char *string, struct parser_variable *variable)
{
    uint32_t color;

    if (string[0] != '#') {
        return 1;
    }

    for (color = 0; isxdigit(string[0]); ) {
        color *= 16;
        color += isdigit(string[0]) ? string[0] - '0' :
            tolower(string[0]) + 0xa - 'a';
    }

    variable->value.color = color;

    return 0;
}

/* Parse key modifiers, e.g.: Control+Shift. */
static int parse_modifiers(char *modifiers, struct parser_variable *variable)
{
    char *next_modifier, *end;
    uint32_t translation_index;

    variable->value.modifiers = 0;
    while (modifiers != NULL) {
        next_modifier = modifiers;
        while (isalpha(modifiers[0])) {
            modifiers++;
        }
        end = modifiers;

        while (isspace(modifiers[0])) {
            modifiers++;
        }

        if (modifiers[0] == '\0') {
            modifiers = NULL;
        } else if (modifiers[0] != '+') {
            return 1;
        }

        end[0] = '\0';

        translation_index = (uint32_t) -1;
        for (uint32_t i = 0; i < SIZE(modifier_translations); i++) {
            if (strcmp(modifier_translations[i].name, next_modifier) == 0) {
                translation_index = i;
                break;
            }
        }
        if (translation_index == (uint32_t) -1) {
            return 1;
        }

        variable->value.modifiers |=
            modifier_translations[translation_index].modifier;
    }
    return 0;
}

/* Parse a key bind, e.g.: a next-window. */
static int parse_key(char *key, struct parser_variable *variable)
{
    char *next_value;
    bool is_end;
    uint32_t translation_index;

    variable->value.key.modifiers = 0;
    while (1) {
        next_value = key;
        while (isalpha(key[0])) {
            key++;
        }

        if (key[0] == '\0') {
            return 1;
        }

        is_end = isspace(key[0]);

        key[0] = '\0';
        key++;

        if (is_end) {
            variable->value.key.key_symbol = XStringToKeysym(next_value);
            break;
        }

        translation_index = (uint32_t) -1;
        for (uint32_t i = 0; i < SIZE(modifier_translations); i++) {
            if (strcmp(modifier_translations[i].name, next_value) == 0) {
                translation_index = i;
                break;
            }
        }
        if (translation_index == (uint32_t) -1) {
            return 1;
        }

        variable->value.key.modifiers |=
            modifier_translations[translation_index].modifier;
    }

    next_value = key;
    while (isalpha(key[0]) || key[0] == '-') {
        key++;
    }

    if (key[0] != '\0') {
        key[0] = '\0';

        do {
            key++;
        } while (isspace(key[0]));

        if (key[0] != '\0') {
            return 1;
        }
    }

    variable->value.key.action = convert_string_to_action(next_value);
    if (variable->value.key.action == ACTION_NULL) {
        return 1;
    }

    return 0;
}

static void update_configuration_value(struct configuration *configuration,
        merge_t merge, struct parser_variable *variable)
{
    switch (merge) {
    /* do nothing */
    case MERGE_NONE:
        break;

    case MERGE_FONT_NAME:
        free(configuration->font.name);
        configuration->font.name = variable->value.name;
        break;

    case MERGE_BORDER_SIZE:
        configuration->border.size = variable->value.integer;
        break;
    case MERGE_BORDER_COLOR:
        configuration->border.color = variable->value.color;
        break;
    case MERGE_BORDER_FOCUS_COLOR:
        configuration->border.focus_color = variable->value.color;
        break;

    case MERGE_GAPS_SIZE:
        configuration->gaps.size = variable->value.integer;
        break;

    case MERGE_KEYBOARD_MAIN_MODIFIERS:
        configuration->keyboard.main_modifiers = variable->value.modifiers;
        break;
    case MERGE_KEYBOARD_IGNORE_MODIFIERS:
        configuration->keyboard.ignore_modifiers = variable->value.modifiers;
        break;

    /* handled elsewhere */
    case MERGE_KEYBOARD_KEYS:
        break;
    }
}

/* Parses and handles given textual line. */
int parse_line(struct parser_context *context)
{
    static const char *labels[PARSER_LABEL_MAX] = {
        [PARSER_LABEL_FONT] = "font",
        [PARSER_LABEL_BORDER] = "border",
        [PARSER_LABEL_GAPS] = "gaps",
        [PARSER_LABEL_KEYBOARD] = "keyboard",
    };

    static const struct variable {
        const char *name;
        int (*parser)(char *argument, struct parser_variable *variable);
        merge_t merge;
    } variables[PARSER_LABEL_MAX][8] = {
        [PARSER_LABEL_FONT] = {
            { "name", parse_name, MERGE_FONT_NAME },
        },

        [PARSER_LABEL_BORDER] = {
            { "size", parse_integer, MERGE_BORDER_SIZE },
            { "color", parse_color, MERGE_BORDER_COLOR },
            { "focus-color", parse_color, MERGE_BORDER_FOCUS_COLOR },
        },

        [PARSER_LABEL_GAPS] = {
            { "size", parse_integer, MERGE_GAPS_SIZE },
        },

        [PARSER_LABEL_KEYBOARD] = {
            { "main-modifiers", parse_modifiers, MERGE_KEYBOARD_MAIN_MODIFIERS },
            { "ignore-modifiers", parse_modifiers, MERGE_KEYBOARD_IGNORE_MODIFIERS },
        },
    };

    char *line;
    char *word, *end_word;

    struct parser_variable variable;
    merge_t merge;

    line = context->line;

    /* remove leading whitespace */
    while (isblank(line[0])) {
        line++;
    }
    
    /* ignore empty lines */
    if (line[0] == '\0') {
        return 0;
    }

    /* get an identifier */
    word = line;
    while (isalnum(line[0]) || line[0] == '-') {
        line++;
    }

    /* check for a label */
    if (line[0] == ':') {
        line[0] = '\0';
        for (label_t i = PARSER_FIRST_LABEL; i < PARSER_LABEL_MAX; i++) {
            if (strcmp(labels[i], word) == 0) {
                context->label = i;
                return 0;
            }
        }
        return 1;
    }

    /* if this is the end of the string already, then there is no hope */
    if (line[0] == '\0') {
        return 1;
    }

    end_word = line;
    end_word[0] = '\0';

    do {
        line++;
    } while (isspace(line[0]));

    for (uint32_t i = 0; i < SIZE(variables[context->label]); i++) {
        if (variables[context->label][i].name == NULL) {
            break;
        }

        if (strcmp(variables[context->label][i].name, word) == 0) {
            if (variables[context->label][i].parser(line, &variable) != 0) {
                return 1;
            }
            merge = variables[context->label][i].merge;
            update_configuration_value(context->configuration, merge, &variable);
            context->merge |= merge;
            return 0;
        }
    }

    if (context->label == PARSER_LABEL_KEYBOARD) {
        end_word[0] = ' ';
        if (parse_key(word, &variable) != 0) {
            return 1;
        }
        RESIZE(context->configuration->keyboard.keys,
                context->configuration->keyboard.number_of_keys + 1);
        context->configuration->keyboard.keys[
            context->configuration->keyboard.number_of_keys++] = variable.value.key;
        context->merge |= MERGE_KEYBOARD_KEYS;
        return 0;
    }

    return 1;
}
