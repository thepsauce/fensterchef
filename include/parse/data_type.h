#ifndef PARSE__DATA_TYPE_H
#define PARSE__DATA_TYPE_H

#include <stddef.h>

#include "bits/binding.h"

#include "utility/types.h"

/* integer type the parser should use */
typedef int_fast32_t parse_integer_t;
#define PRIiPARSE_INTEGER PRIiFAST32

/* If the integer is a percentage of something.  For example this might be 20%
 * off the width of a monitor.
 */
#define PARSE_DATA_FLAGS_IS_PERCENT (1 << 0)

/* all data types */
typedef enum parse_data_type {
    PARSE_DATA_TYPE_INTEGER,
    PARSE_DATA_TYPE_STRING,
    PARSE_DATA_TYPE_BUTTON,
    PARSE_DATA_TYPE_KEY,
} parse_data_type_t;

/* generic action data */
struct parse_generic_data {
    /* an OR combination of `PARSE_DATA_FLAGS_*` */
    unsigned flags;
    parse_data_type_t type;
    /* the literal value */
    union {
        /* integer value */
        parse_integer_t integer;
        /* a nul-terminated string */
        utf8_t *string;
        /* a button binding */
        struct button_binding button;
        /* a key binding */
        struct key_binding key;
    } u;
};

#endif
