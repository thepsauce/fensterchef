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

/* If the integer is a pixel value.  When talking about units, 1 != 1px.
 * The first "1" is a device independent 1 that is combined with the monitors
 * DPI.  The second "1px" is always 1 pixel, no matter the device.
 */
#define PARSE_DATA_FLAGS_IS_PIXEL (1 << 1)

/* If the data must be freed. */
#define PARSE_DATA_FLAGS_IS_POINTER (1 << 2)

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
