#ifndef CONFIGURATION_PARSER_ERROR_H
#define CONFIGURATION_PARSER_ERROR_H

#include "utility.h"

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
    X(PARSER_UNEXPECTED, "unexpected tokens") \
    /* this is used when there is definitely an error */ \
    X(PARSER_ERROR_UNEXPECTED, "unexpected tokens") \
    /* trailing characters after a correctly parsed lined */ \
    X(PARSER_ERROR_TRAILING, "trailing characters") \
    /* when parsing a string, there was a backslash that escaped nothing */ \
    X(PARSER_ERROR_TRAILING_BACKSLASH, "trailing backslash") \
    /* the identifier exceeds the limit */ \
    X(PARSER_ERROR_TOO_LONG, "identifier exceeds identifier limit " \
       STRINGIFY(PARSER_IDENTIFIER_LIMIT)) \
    /* include files go too deep (or cycle) */ \
    X(PARSER_ERROR_INCLUDE_OVERFLOW, "too hight include depth") \
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
    X(PARSER_ERROR_INVALID_ACTION, "invalid action value") \
    /* a separator (';') was expected */ \
    X(PARSER_ERROR_EXPECTED_SEPARATOR, "expected separator ';'") \
    /* an unsigned integer was expected */ \
    X(PARSER_ERROR_EXPECTED_UNSIGNED_INTEGER, "expected an unsigned integer") \
    /* a data type does not support an operation */ \
    X(PARSER_ERROR_INVALID_OPERATOR, "operator not defined for this data type") \

/* parser error codes */
typedef enum {
#define X(error, string) error,
    DEFINE_ALL_CONFIGURATION_PARSER_ERRORS
#undef X
} parser_error_t;

#endif
