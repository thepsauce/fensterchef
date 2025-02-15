#ifndef CONFIGURATION_PARSER_DATA_TYPE_H
#define CONFIGURATION_PARSER_DATA_TYPE_H

#include <stdbool.h>
#include <stdint.h>

/* This had to be moved into bits/ because both action.h and
 * configuration_parser.h need it and there were unresolvable intersections
 * between the header files.
 */

/* data types the parser understands
 *
 * NOTE: After editing a data type, also edit the data_type_parsers[] array in
 * configuration_parser.c and implement its parser function. It can then be used
 * in parse_line().
 */
typedef enum parser_data_type {
    /* no data type at all */
    PARSER_DATA_TYPE_VOID,
    /* true or false, in text one of: on yes true off no false */
    PARSER_DATA_TYPE_BOOLEAN,
    /* any text without leading or trailing space */
    PARSER_DATA_TYPE_STRING,
    /* an integer in simple decimal notation */
    PARSER_DATA_TYPE_INTEGER,
    /* color in the format (X: hexadecimal digit): #XXXXXX */
    PARSER_DATA_TYPE_COLOR,
    /* key modifiers, e.g.: Control+Shift */
    PARSER_DATA_TYPE_MODIFIERS,
} parser_data_type_t;

/* the value of a data type */
union parser_data_value {
    /* void has no data value */
    // pseudo code: void void_;
    /* true or false, in text one of: on yes true off no false */
    bool boolean;
    /* any utf8 text without leading or trailing space */
    uint8_t *string;
    /* an integer in simple decimal notation */
    uint32_t integer;
    /* color in the format (X: hexadecimal digit): #XXXXXX */
    uint32_t color;
    /* key modifiers, e.g.: Control+Shift */
    uint16_t modifiers;
};

/*** Implemented in configuration_parser.c ***/

/* Duplicates given @value deeply into itself. */
void duplicate_data_value(parser_data_type_t type,
        union parser_data_value *value);

/* Frees the resources the given data value occupies. */
void clear_data_value(parser_data_type_t type, union parser_data_value *value);

#endif
