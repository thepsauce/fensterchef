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

/* Get the number of digits this number could take up.
 *
 * We just consider the number of bytes the number needs and since a single byte
 * goes from 0 to 255 (3 digits), multiplying by 3 is rounding up a little but
 * it is also very safe.
 */
#define APPROXIMATE_DIGITS(number_type) (sizeof(number_type) * 3)

/* Get the size of a statically sized array. */
#define SIZE(a) (sizeof(a) / sizeof(*(a)))

/* Turn the argument into a string. */
#define _STRINGIFY(str) #str
#define STRINGIFY(str) _STRINGIFY(str)

/* Resize allocated array to given number of elements.
 *
 * Example usage:
 * ```
 * uint32_t *integers = xmalloc(sizeof(*integers) * 60);
 *
 * integers[59] = 64;
 *
 * RESIZE(integers, 120);
 *
 * integers[119] = 8449;
 * ```
 */
#define RESIZE(p, a) ((p) = xreallocarray(p, (a), sizeof(*(p))))

/* Get the maximum of two numbers. */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Get the minimum of two numbers. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get the absolute difference between two numbers. */
#define ABSOLUTE_DIFFERENCE(a, b) ((a) < (b) ? (b) - (a) : (a) - (b))

/* Check if the multiplication overflows and store the result in @c. */
#define OVERFLOW_MULTIPLY(a, b, c) \
    __builtin_mul_overflow((a), (b), &(c))

/* Check if the addition overflows and store the result in @c. */
#define OVERFLOW_ADD(a, b, c) \
    __builtin_add_overflow((a), (b), &(c))

/* Multiply two numbers @a and @b without exceeding @maximum.
 *
 * The result is stored back in @a, so it must be writable.
 */
#define CLIP_MULTIPLY(a, b, maximum) do { \
    typeof(a) _c; \
    if (OVERFLOW_MULTIPLY((a), (b), _c)) { \
        (a) = (maximum); \
    } else { \
        (a) = MIN(_c, (maximum)); \
    } \
} while (false)

/* a point at position x, y */
typedef struct position {
    /* horizontal position */
    int32_t x;
    /* vertical position */
    int32_t y;
} Point;

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

/* fraction: numerator over denominator */
typedef struct ratio {
    uint32_t numerator;
    uint32_t denominator;
} Ratio;

/* Get the length of @string up to a maximum of @max_length. */
size_t strnlen(const char *string, size_t max_length);

/* Compare two strings and ignore case.
 *
 * @return 0 for equality, a negative value if @string1 < @string2,
 *         otherwise a positive value.
 */
int strcasecmp(const char *string1, const char *string2);

/* Matches a string against a pattern.
 *
 * Pattern metacharacters are ?, *, [ and \.
 * (And, inside character classes, ^, - and ].)
 *
 * An opening bracket without a matching close is matched literally.
 *
 * @pattern is a shell-style pattern, e.g. "*.[ch]".
 *
 * @return if the string matches the pattern.
 */
bool matches_pattern(char const *pattern, char const *string);

#endif
