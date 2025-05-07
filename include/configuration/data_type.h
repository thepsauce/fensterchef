#ifndef CONFIGURATION__DATA_TYPE_H
#define CONFIGURATION__DATA_TYPE_H

#include <stddef.h>

#include "utility/utility.h"

/* align data to pointer boundaries */
typedef ptrdiff_t parse_data_align_t;

/* If the integer is a percentage of something.  For example this might be 20%
 * off the width of a monitor.
 */
#define PARSE_DATA_FLAGS_IS_PERCENT (1 << 0)

/* If the data must be freed. */
#define PARSE_DATA_FLAGS_IS_POINTER (1 << 1)

/* generic action data */
struct parse_generic_data {
    /* an OR combination of `PARSE_DATA_FLAGS_*` */
    parse_data_align_t flags;
    /* the literal value */
    union {
        /* integer value */
        parse_data_align_t integer;
        /* a nul-terminated string */
        utf8_t *string;
        /* a pointer used for freeing pointer values like `string` */
        void *pointer;
    } u;
};

#endif
