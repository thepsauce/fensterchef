#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>

#include <stdbool.h>
#include <stdint.h>

#include "xalloc.h"

/* no error */
#define OK 0
/* indicate integer error value */
#define ERROR 1

/* Get the size of a statically sized array. */
#define SIZE(a) (sizeof(a)/sizeof(*(a)))

/* Turns the first argument into a string. */
#define STRINGIFY(str) #str

/* Resize allocated memory to given size.
 *
 * Usage:
 * ```
 * uint32_t *integers = xmalloc(60);
 *
 * RESIZE(integers, 120);
 *
 * integers[0] = 64;
 * ```
 */
#define RESIZE(p, a) \
    ((p) = xreallocarray(p, (a), sizeof(*(p))));

/* Get the maximum of two numbers. */
#define MAX(a, b) ((a)>(b)?(a):(b))

/* Get the minimum of two numbers. */
#define MIN(a, b) ((a)<(b)?(a):(b))

/* Get the absolute difference of tow numbers. */
#define ABSOLUTE_DIFFERENCE(a, b) (MAX(a,b)-MIN(a,b))

/* a = MIN(a * b, c) */
#define CLIP_MULTIPLY(a, b, c) do { \
    typeof(a) _c; \
    if (__builtin_mul_overflow((a), (b), &_c)) { \
        (a) = (c); \
    } else { \
        (a) = MIN(_c, c); \
    } \
} while (0)

/* Check if the multiplication overflows and store the result in @c. */
#define OVERFLOW_MULTIPLY(a, b, c) \
    __builtin_mul_overflow((a), (b), &(c))

/* Check if the addition overflows and store the result in @c. */
#define OVERFLOW_ADD(a, b, c) \
    __builtin_add_overflow((a), (b), &(c))

/* a point at position x, y */
typedef struct position {
    /* horizontal position */
    int32_t x;
    /* vertical position */
    int32_t y;
} Position;

/* a size of width x height */
typedef struct size {
    /* horizontal size */
    uint32_t width;
    /* vertical size */
    uint32_t height;
} Size;

/* offsets from the edges of *something* */
typedef struct extents {
    /* left extent */
    int32_t left;
    /* right extent */
    int32_t right;
    /* top extent */
    int32_t top;
    /* bottom extent */
    int32_t bottom;
} Extents;

/* a rectangular region */
typedef struct rectangle {
    /* horizontal position */
    int32_t x;
    /* vertical position */
    int32_t y;
    /* horizontal size */
    uint32_t width;
    /* vertical size */
    uint32_t height;
} Rectangle;

/* Get the length of @string up to a maximum of @max_length. */
size_t strnlen(const char *string, size_t max_length);

/* Compare two strings and ignore case.
 *
 * @return 0 for equality, a negative value if str1 < str2, otherwise a positive
 * value.
 */
int strcasecmp(const char *string1, const char *string2);

#endif
