#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>
#include <stdint.h>

#include "cursor.h"
#include "utf8.h"

/* data types the parser understands
 *
 * NOTE: After editing a data type, also edit the data_types[] array in
 * configuration_parser.c and implement its parser function. It will then be
 * automatically be used in parse_line().
 * The compiler will also complain about certain switches that need to be
 * implemented in data_type.c.
 */
typedef enum data_type {
    /* no data type at all */
    DATA_TYPE_VOID,
    /* true or false, in text one of: on yes true off no false */
    DATA_TYPE_BOOLEAN,
    /* any text without leading or trailing space */
    DATA_TYPE_STRING,
    /* an integer in simple decimal notation */
    DATA_TYPE_INTEGER,
    /* a set of 4 integers */
    DATA_TYPE_QUAD,
    /* color in the format (X: hexadecimal digit): #XXXXXX */
    DATA_TYPE_COLOR,
    /* key modifiers, e.g.: Control+Shift */
    DATA_TYPE_MODIFIERS,
    /* Xcursor constant, e.g. left-ptr */
    DATA_TYPE_CURSOR,

    /* the maximum value of a data type */
    DATA_TYPE_MAX
} data_type_t;

/* generic value of a data type */
typedef union data_type_value {
    /* true or false, in text one of: on yes true off no false */
    bool boolean;
    /* any utf8 text without leading or trailing space */
    uint8_t *string;
    /* an integer in simple decimal notation */
    int32_t integer;
    /* a set of 1, 2 or 4 integers */
    int32_t quad[4];
    /* color in the format (X: hexadecimal digit): #XXXXXX */
    uint32_t color;
    /* key modifiers, e.g.: Control+Shift */
    uint16_t modifiers;
    /* cursor constant, e.g. left-ptr */
    core_cursor_t cursor;
} GenericData;

/* the size in bytes of each data type */
extern size_t data_type_sizes[DATA_TYPE_MAX];

/* Duplicate given @value deeply into itself. */
void duplicate_data_value(data_type_t type, GenericData *data);

/* Free the resources the given data value occupies. */
void clear_data_value(data_type_t type, GenericData *data);

#endif
