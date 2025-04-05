#ifndef CONFIGURATION__PARSER_H
#define CONFIGURATION__PARSER_H

#include <stdio.h>

#include "configuration/default.h"
#include "configuration/label.h"
#include "data_type.h"
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
    /* a line is terminated but tokens were expected first */ \
    X(PARSER_ERROR_PREMATURE_LINE_END, "premature line end") \
    /* invalid number of integers for a quad */ \
    X(PARSER_ERROR_INVALID_QUAD, "invalid quad (need either 1, 2 or 4 integer expressions)") \
    /* invalid syntax for modifiers */ \
    X(PARSER_ERROR_INVALID_MODIFIERS, "invalid modifiers") \
    /* invalid button name */ \
    X(PARSER_ERROR_INVALID_BUTTON, "invalid button name") \
    /* invalid button flag */ \
    X(PARSER_ERROR_INVALID_BINDING_FLAG, "invalid binding flag") \
    /* invalid key code value */ \
    X(PARSER_ERROR_INVALID_KEY_CODE, "invalid key code value") \
    /* invalid key symbol name */ \
    X(PARSER_ERROR_INVALID_KEY_SYMBOL, "invalid key symbol name") \
    /* an action value is missing */ \
    X(PARSER_ERROR_INVALID_ACTION, "invalid action type") \
    /* a separator (';') was expected */ \
    X(PARSER_ERROR_EXPECTED_SEPARATOR, "expected separator ';'") \
    /* an unsigned integer was expected */ \
    X(PARSER_ERROR_INTEGER_TOO_LARGE, "the integer is too big") \
    /* a data type does not support an operation */ \
    X(PARSER_ERROR_INVALID_OPERATOR, "operator not defined for this data type") \
    /* there is a ) but no opening bracket */ \
    X(PARSER_ERROR_MISSING_OPENING_BRACKET, "missing opening '('") \
    /* there is a ( but no closing bracket */ \
    X(PARSER_ERROR_MISSING_CLOSING_BRACKET, "missing closing ')'") \
    /* the wrong type is used */ \
    X(PARSER_ERROR_TYPE_MISMATCH, "the wrong type is used") \
    /* a variable is used without prior declaration */ \
    X(PARSER_ERROR_UNSET_VARIABLE, "the variable is not set") \
    /* there is an attempt to set something not a variable */ \
    X(PARSER_ERROR_MISAPPLIED_SET, "'=' must be applied to a variable") \
    /* there are no more variable slots */ \
    X(PARSER_ERROR_OUT_OF_VARIABLES, "maximum number of variables exceeded")

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
        /* the name of the previous file */
        utf8_t *name;
        /* the pushed file */
        FILE *file;
        /* the current line number within `file` */
        unsigned long line_number;
        /* the label before opening `file` */
        parser_label_t label;
    } file_stack[32];
    /* the number of files on the file stack */
    uint32_t number_of_pushed_files;
    /* a string being read from, this is used if `file` is NULL. */
    const utf8_t *string_source;
    /* the current index within `string_source` */
    size_t string_source_index;
    /* the current line being parsed */
    utf8_t *line;
    /* the number of allocated bytes for the line */
    size_t line_capacity;
    /* the line number the parser is one (1 based) */
    unsigned long line_number;
    /* where the start of the last syntax item is */
    unsigned item_start_column;
    /* the current position on the line */
    unsigned column;
    /* the configuration being filled */
    struct configuration configuration;
    /* the labels that have appeared in the configuration */
    bool has_label[PARSER_LABEL_MAX];
    /* the currently active label */
    parser_label_t label;
    /* the latest parsed identifier */
    utf8_t identifier[PARSER_IDENTIFIER_LIMIT];
    /* a single identifying character like '[' or ']' */
    utf8_t character;

    /** utility for expression parsing **/
    /* the instructions being filled */
    uint32_t *instructions;
    /* number of instructions */
    uint32_t instruction_size;
    /* number of allocated instructions */
    uint32_t instruction_capacity;
    /* the position on the stack */
    uint32_t stack_position;
    /* local variables set; by design, they are sorted ascending with respect to
     * the address they reside in
     */
    struct local {
        /* type of the local variable */
        data_type_t type;
        /* name of the local variable */
        char *name;
        /* address within the stack */
        uint32_t address;
    } *locals;
    /* number of current local variables */
    uint32_t number_of_locals;
    /* number of allocated local variables */
    uint32_t locals_capacity;
} Parser;

/* Converts @error to a string. */
const char *parser_error_to_string(parser_error_t error);

/* Prepare a parser for parsing. */
int initialize_parser(Parser *parser, const char *string, bool is_string_file);

/* Free the resources the parser occupies.
 *
 * This omits freeing @parser->configuration which needs to be handled
 * externally.
 */
void deinitialize_parser(Parser *parser);

/* Read the next line from the parsed files or string source into @parser->line.
 *
 * This also skips over all lines starting with any amount of space and a '#'.
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

/* Parse any text that may include escaped characters.
 *
 * This stops at the separator characters ;, &, | and ).
 *
 * @return PARSER_UNEXPECTED when there was nothing there (just space or an
 *         immediate separator).
 */
parser_error_t parse_string(Parser *parser, utf8_t **output);

/* Parse an expression.
 *
 * The parsed expression is appended to @parser->instructions.
 */
parser_error_t parse_expression_and_append(Parser *parser);

/* Parse 1, 2 or 4 four expression in series.
 *
 * The expression is prefixed with the QUAD literal instruction and put at the
 * end of @parser->instructions.
 */
parser_error_t parse_quad_expression_and_append(Parser *parser);

/* Put the internal expression parsing state back to the start. */
void reset_expression(Parser *parser);

/* Parse and evaluate the next line within the parser.
 *
 * @return PARSER_SUCCESS for success, otherwise an error code.
 */
parser_error_t parse_line(Parser *parser);

#endif
