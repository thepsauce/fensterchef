#ifndef CONFIGURATION_PARSER_H
#define CONFIGURATION_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "bits/configuration_parser_label.h"

#include "data_type.h"
#include "default_configuration.h"
#include "utility.h"

/* maximum length of an identifier */
#define PARSER_IDENTIFIER_LIMIT 64

/* maximum value for an integer */
#define PARSER_INTEGER_LIMIT 1000000

/* expands to all possible configuration parser errors */
#define DEFINE_ALL_CONFIGURATION_PARSER_ERRORS \
    /* indicates a successful parsing */ \
    X(PARSER_SUCCESS, "success") \
\
    /* this may or may not be an error; if for instance an integer is expected
     * and a string is given, this would be an error; however, when an integer
     * is expected, unexpected tokens appear but the argument is optional, no
     * error is raised
     */ \
    X(PARSER_UNEXPECTED, "unexpected token") \
    /* this is used when there is definitely an error */ \
    X(PARSER_ERROR_UNEXPECTED, "unexpected token") \
    /* trailing characters after a correctly parsed lined */ \
    X(PARSER_ERROR_TRAILING, "trailing characters") \
    /* when parsing a string, there was a backslash that escaped nothing */ \
    X(PARSER_ERROR_TRAILING_BACKSLASH, "trailing backslash") \
    /* the identifier exceeds the limit */ \
    X(PARSER_ERROR_TOO_LONG, "identifier exceeds identifier limit " \
       STRINGIFY(PARSER_IDENTIFIER_LIMIT)) \
    /* include files go too deep (or cycle) */ \
    X(PARSER_ERROR_INCLUDE_OVERFLOW, "too high include depth") \
    /* a file could not be included because it is missing or it has bad file
     * permissions
     */ \
    X(PARSER_ERROR_INVALID_INCLUDE, "could not include file") \
    /* label does not exist */ \
    X(PARSER_ERROR_INVALID_LABEL, "invalid label name") \
    /* a ']' is missing */ \
    X(PARSER_ERROR_MISSING_CLOSING, "missing a closing ']'") \
    /* invalid boolean identifier */ \
    X(PARSER_ERROR_INVALID_BOOLEAN, "invalid boolean value") \
    /* a label does not define given variable name */ \
    X(PARSER_ERROR_INVALID_VARIABLE_NAME, "the label does not have that variable name") \
    /* color is not in the right format */ \
    X(PARSER_ERROR_BAD_COLOR_FORMAT, "bad color format (expect #XXXXXX)") \
    /* a line is terminated but tokens were expected first */ \
    X(PARSER_ERROR_PREMATURE_LINE_END, "premature line end") \
    /* invalid number of integers for a quad */ \
    X(PARSER_ERROR_INVALID_QUAD, "invalid quad (either 1, 2 or 4 integers)") \
    /* invalid syntax for modifiers */ \
    X(PARSER_ERROR_INVALID_MODIFIERS, "invalid modifiers") \
    /* invalid cursor name */ \
    X(PARSER_ERROR_INVALID_CURSOR, "invalid cursor name") \
    /* invalid button name */ \
    X(PARSER_ERROR_INVALID_BUTTON, "invalid button name") \
    /* invalid button flag */ \
    X(PARSER_ERROR_INVALID_BUTTON_FLAG, "invalid button flag") \
    /* invalid key symbol name */ \
    X(PARSER_ERROR_INVALID_KEY_SYMBOL, "invalid key symbol name") \
    /* invalid value for an action */ \
    X(PARSER_ERROR_MISSING_ACTION, "action value is missing") \
    /* an action value is missing */ \
    X(PARSER_ERROR_INVALID_ACTION, "invalid action type") \
    /* a separator (';') was expected */ \
    X(PARSER_ERROR_EXPECTED_SEPARATOR, "expected separator ';'") \
    /* an unsigned integer was expected */ \
    X(PARSER_ERROR_EXPECTED_UNSIGNED_INTEGER, "expected an unsigned integer") \
    /* a data type does not support an operation */ \
    X(PARSER_ERROR_INVALID_OPERATOR, "operator not defined for this data type") \
    /* there is a ) but no opening bracket */ \
    X(PARSER_ERROR_MISSING_OPENING_BRACKET, "missing opening '('") \
    /* there is a ( but no closing bracket */ \
    X(PARSER_ERROR_MISSING_CLOSING_BRACKET, "missing closing ')'") \
    /* the wrong type is used */ \
    X(PARSER_ERROR_TYPE_MISMATCH, "the wrong type is used") \

/* parser error codes */
typedef enum {
#define X(error, string) error,
    DEFINE_ALL_CONFIGURATION_PARSER_ERRORS
#undef X
} parser_error_t;

/* the state of a parser */
typedef struct parser {
    /* the file being read from */
    FILE *file;
    /* stack of include files */
    struct parser_file_stack_entry {
        /* the label before opening `file` */
        parser_label_t label;
        /* the deeper file */
        FILE *file;
    } file_stack[32];
    /* the number of files on the file stack */
    uint32_t number_of_pushed_files;
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
    /* the same identifier but in lower case */
    char identifier_lower[PARSER_IDENTIFIER_LIMIT];
    /* a single identifying character like '[' or ']' */
    char character;
} Parser;

/* Converts @error to a string. */
const char *parser_error_to_string(parser_error_t error);

/* Translate a string like "shift" to a modifier bit. */
int string_to_modifier(const char *string, uint16_t *output);

/* Translate a string like "false" to a boolean value. */
int string_to_boolean(const char *string, bool *output);

/* Read the next line from the file.
 *
 * @return true if there is any line more lines, otherwise false.
 */
bool read_next_line(Parser *context);

/* Skip over empty characters (space). */
void skip_space(Parser *parser);

/* Skip leading space and put the next character into @parser->character. */
parser_error_t parse_character(Parser *parser);

/* Skip leading space and load the next identifier into @parser->identifier.
 *
 * A lower case variant is loaded into @parser->identifier_lower.
 */
parser_error_t parse_identifier(Parser *parser);

/* Parse an integer in simple decimal notation. */
parser_error_t parse_integer(Parser *parser, int32_t *output);

/* Parse any text that may include escaped characters.
 *
 * Note that this stops at a semicolon.
 */
parser_error_t parse_string(Parser *parser, utf8_t **output);

/* Parse an expression. */
parser_error_t parse_expression(Parser *parser, Expression *expression);

/* Parse and evaluate the next line within the parser.
 *
 * @return PARSER_SUCCESS for success, otherwise an error code.
 */
parser_error_t parse_line(Parser *parser);

#endif
