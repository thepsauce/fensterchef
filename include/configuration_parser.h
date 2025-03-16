#ifndef CONFIGURATION_PARSER_H
#define CONFIGURATION_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "bits/configuration_parser_data_type.h"
#include "bits/configuration_parser_error.h"
#include "bits/configuration_parser_label.h"

#include "default_configuration.h"
#include "utility.h"

/* maximum length of an identifier */
#define PARSER_IDENTIFIER_LIMIT 64

/* maximum value for an integer */
#define PARSER_INTEGER_LIMIT 1000000

/* the state of a parser */
typedef struct parser {
    /* the file being read from */
    FILE *file;
    /* a string being read from, this is used if `file` is NULL. */
    const char *string_source;
    /* the current index within `string_source` */
    size_t string_source_index;
    /* the current line being parsed */
    char *line;
    /* the number of allocated bytes for the line */
    size_t line_capacity;
    /* the line number the parser is one (1 based) */
    size_t line_number;
    /* where the start of the last syntax item is */
    size_t item_start_column;
    /* the current position on the line */
    size_t column;
    /* the configuration being filled */
    struct configuration configuration;
    /* the labels that have appeared in the configuration */
    bool has_label[PARSER_LABEL_MAX];
    /* the currently active label */
    parser_label_t label;

    /* the latest parsed identifier */
    char identifier[PARSER_IDENTIFIER_LIMIT];
    /* a single identifying character like '[' or ']' */
    char character;

    /* parsed data value */
    union parser_data_value data;
    /* mousebinding, e.g.: MiddleButton close-window */
    struct configuration_button button;
    /* keybinding, e.g.: n next-window */
    struct configuration_key key;
} Parser;

/* Converts @error to a string. */
const char *parser_error_to_string(parser_error_t error);

/* Read the next line from the file.
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
