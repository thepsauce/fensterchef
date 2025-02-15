#ifndef CONFIGURATION_PARSER_H
#define CONFIGURATION_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "bits/configuration_parser_data_type.h"

#include "configuration.h"
#include "utility.h"

#define PARSER_IDENTIFIER_LIMIT 64

/* parser error codes
 * 
 * NOTE: After editing an error code, also edit the parser_error_strings[] in
 * configuration_parser.c.
 */
typedef enum {
    /* indicates a successful parsing */
    PARSER_SUCCESS,

    /* trailing characters after a correctly parsed lined */
    PARSER_ERROR_TRAILING,
    /* the identifier exceeds the limit */
    PARSER_ERROR_TOO_LONG,
    /* label does not exist */
    PARSER_ERROR_INVALID_LABEL,
    /* a ']' is missing */
    PARSER_ERROR_MISSING_CLOSING,
    /* no label was specified */
    PARSER_ERROR_NOT_IN_LABEL,
    /* invalid boolean identifier */
    PARSER_ERROR_INVALID_BOOLEAN,
    /* a label does not define given variable name */
    PARSER_ERROR_INVALID_VARIABLE_NAME,
    /* color is not in the right format */
    PARSER_ERROR_BAD_COLOR_FORMAT,
    /* a line is terminated but tokens were expected first */
    PARSER_ERROR_PREMATURE_LINE_END,
    /* invalid syntax for modifiers */
    PARSER_ERROR_INVALID_MODIFIERS,
    /* invalid key symbol name */
    PARSER_ERROR_INVALID_KEY_SYMBOL,
    /* invalid value for an action */
    PARSER_ERROR_INVALID_ACTION,
    /* an action value is missing */
    PARSER_ERROR_MISSING_ACTION,
    /* an unexpected syntax on a line */
    PARSER_ERROR_UNEXPECTED
} parser_error_t;

/* labels with form "[<name>]"
 *
 * NOTE: After editing this enum, also edit the label_strings[] array in
 * configuration_parser.c.
 * To add variables to the label, edit variables[] in parse_line().
 */
typedef enum parser_label {
    PARSER_LABEL_NONE,
    
    PARSER_FIRST_LABEL,

    PARSER_LABEL_GENERAL = PARSER_FIRST_LABEL,
    PARSER_LABEL_TILING,
    PARSER_LABEL_FONT,
    PARSER_LABEL_BORDER,
    PARSER_LABEL_GAPS,
    PARSER_LABEL_NOTIFICATION,
    PARSER_LABEL_KEYBOARD,

    PARSER_LABEL_MAX,
} parser_label_t;

/* the state of a parser */
typedef struct parser {
    /* the file being read from */
    FILE *file;
    /* the current line being parsed */
    char *line;
    /* the number of allocated bytes for @line */
    size_t line_capacity;
    /* the line number the parser is one (1 based) */
    size_t line_number;
    /* where the start of the last syntax item is */
    size_t item_start_column;
    /* the current position on the line */
    size_t column;
    /* the configuration being filled */
    struct configuration *configuration;
    /* the currently active label */
    parser_label_t label;

    /* the latest parser identifier */
    char identifier[PARSER_IDENTIFIER_LIMIT];
    /* a single identifying character like '[' or ']' */
    char character;

    /* parsed data value */
    union parser_data_value data;
    /* keybinding, e.g.: n next-window */
    struct configuration_key key;
} Parser;

/* Converts @error to a string. */
const char *parser_string_error(parser_error_t error);

/* Read the next line from the file.
 *
 * Skips any empty lines.
 *
 * @return if there is any line left.
 */
bool read_next_line(Parser *context);

/* Parse and evaluate the next line within the parser.
 *
 * @return PARSER_SUCCESS for success or an error code.
 */
parser_error_t parse_line(Parser *parser);

#endif
