#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>
#include <stdint.h>

#include "bits/frame_typedef.h"
#include "bits/window_typedef.h"

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
    /* any utf8 text without any leading or trailing spaces */
    DATA_TYPE_STRING,
    /* an integer in simple decimal notation */
    DATA_TYPE_INTEGER,
    /* a set of 1, 2 or 4 integers */
    DATA_TYPE_QUAD,

    /* the maximum value of a data type */
    DATA_TYPE_MAX
} data_type_t;

/* generic value of a data type */
typedef union data_type_value {
    /* any utf8 text without any leading or trailing spaces */
    utf8_t *string;
    /* an integer in simple decimal notation */
    int32_t integer;
    /* a set of 1, 2 or 4 integers */
    int32_t quad[4];
} GenericData;

/* the size in bytes of each data type */
extern size_t data_type_sizes[DATA_TYPE_MAX];

/* Duplicate given @value deeply into itself. */
void duplicate_data_value(data_type_t type, GenericData *data);

/* Free the resources the given data value occupies. */
void clear_data_value(data_type_t type, GenericData *data);

#endif
