#ifndef CONFIGURATION_PARSER_H
#define CONFIGURATION_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "configuration.h"

/* labels with form "<name>:" */
typedef enum {
    PARSER_LABEL_NONE,
    
    PARSER_FIRST_LABEL,

    PARSER_LABEL_FONT = PARSER_FIRST_LABEL,
    PARSER_LABEL_BORDER,
    PARSER_LABEL_GAPS,
    PARSER_LABEL_KEYBOARD,

    PARSER_LABEL_MAX,
} label_t;

/* data types the parser supports */
typedef enum {
    /* any text without leading or trailing space */
    PARSER_TYPE_NAME,
    /* an integer in simple decimal notation */
    PARSER_TYPE_INTEGER,
    /* color in the format (X: hexadecimal digit): #XXXXXX */
    PARSER_TYPE_COLOR,
    /* key modifiers, e.g.: Control+Shift */
    PARSER_TYPE_MODIFIERS,
    /* key bind, e.g.: a next-window */
    PARSER_TYPE_KEY,
} parser_type_t;

/* value, can be of many forms given above */
union parser_value {
    /* PARSER_TYPE_NAME */
    FcChar8 *name;
    /* PARSER_TYPE_INTEGER */
    uint32_t integer;
    /* PARSER_TYPE_COLOR */
    uint32_t color;
    /* PARSER_TYPE_MODIFIERS */
    uint16_t modifiers;
    /* PARSER_TYPE_KEY */
    struct configuration_key key;
};

/* a variable of the form "name value" */
struct parser_variable {
    /* type of the variable */
    parser_type_t type;
    /* value of the variable */
    union parser_value value;
};

/* the state of a parser */
struct parser_context {
    /* the file being read from */
    FILE *file;
    /* the current line being parsed */
    char *line;
    /* the number of allocated bytes for @line */
    size_t line_capacity;
    /* the configuration being filled */
    struct configuration *configuration;
    /* the fields of configuration updated */
    merge_t merge;
    /* the currently active label */
    label_t label;
};

/* Read the next line from the file.
 *
 * Skips any empty lines.
 *
 * @return if there is any line left.
 */
bool read_next_line(struct parser_context *context);

/* Parse and evaluate the next line within the parser.
 *
 * @return 1 if the line is invalid, 0 otherwise.
 */
int parse_line(struct parser_context *context);

#endif
