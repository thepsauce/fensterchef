#ifndef CONFIGURATION_PARSER_ERROR_H
#define CONFIGURATION_PARSER_ERROR_H

#include "utility.h"

/* expands to all possible configuration parser errors */
#define DEFINE_ALL_CONFIGURATION_PARSER_ERRORS \
    /* indicates a successful parsing */ \
    X(PARSER_SUCCESS, "success") \
 \
    /* trailing characters after a correctly parsed lined */ \
    X(PARSER_ERROR_TRAILING, "trailing characters") \
    /* the identifier exceeds the limit */ \
    X(PARSER_ERROR_TOO_LONG, "identifier exceeds identifier limit " \
       STRINGIFY(PARSER_IDENTIFIER_LIMIT)) \
    /* label does not exist */ \
    X(PARSER_ERROR_INVALID_LABEL, "invalid label name") \
    /* a ']' is missing */ \
    X(PARSER_ERROR_MISSING_CLOSING, "missing a closing ']'") \
    /* no label was specified */ \
    X(PARSER_ERROR_NOT_IN_LABEL, "not in a label yet, use `[<label>]` on a previous line") \
    /* invalid boolean identifier */ \
    X(PARSER_ERROR_INVALID_BOOLEAN, "invalid boolean value") \
    /* a label does not define given variable name */ \
    X(PARSER_ERROR_INVALID_VARIABLE_NAME, "the current label does not have that variable name") \
    /* color is not in the right format */ \
    X(PARSER_ERROR_BAD_COLOR_FORMAT, "bad color format (expect #XXXXXX)") \
    /* a line is terminated but tokens were expected first */ \
    X(PARSER_ERROR_PREMATURE_LINE_END, "premature line end") \
    /* invalid number of integers for a quad */ \
    X(PARSER_ERROR_INVALID_QUAD, "invalid quad (either 1, 2 or 4 integers)") \
    /* invalid syntax for modifiers */ \
    X(PARSER_ERROR_INVALID_MODIFIERS, "invalid modifiers") \
    /* invalid button name */ \
    X(PARSER_ERROR_INVALID_BUTTON, "invalid button name") \
    /* invalid button flag */ \
    X(PARSER_ERROR_INVALID_BUTTON_FLAG, "invalid button flag") \
    /* invalid key symbol name */ \
    X(PARSER_ERROR_INVALID_KEY_SYMBOL, "invalid key symbol name") \
    /* invalid value for an action */ \
    X(PARSER_ERROR_MISSING_ACTION, "action value is missing") \
    /* an action value is missing */ \
    X(PARSER_ERROR_INVALID_ACTION, "invalid action value") \
    /* an unexpected syntax on a line */ \
    X(PARSER_ERROR_UNEXPECTED, "unexpected tokens")

/* parser error codes */
typedef enum {
#define X(error, string) error,
    DEFINE_ALL_CONFIGURATION_PARSER_ERRORS
#undef X
} parser_error_t;

#endif
