#ifndef UTIL_H
#define UTIL_H

/* Various utility macros and data types. */

#include <ctype.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "xalloc.h"

typedef char utf8_t;

/* success indicator value */
#define OK 0

/* indicate integer error value */
#define ERROR 1

/* Abort the program after printing an error message. */
#define ABORT(message) do { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, (message)); \
    abort(); \
} while (false)

/* Wrap these around statements when an if branch is involved to hint to the
 * compiler whether an if statement is likely (or unlikely) to be true.
 * For example:
 *   if (UNLIKELY(pointer == NULL)) {
 *       printf("I am unlikely to occur\n");
 *   }
 * These should ONLY be used when it is guaranteed that a branch is executed
 * only very rarely.
 */
#ifdef __GNUC__
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

/* Assert that statement @x is true. If this is not the case, the program is
 * aborted.
 */
#define ASSERT(x, message) do { \
    if (UNLIKELY(!(x))) { \
        ABORT(message); \
    } \
} while (false)

/* Get the maximum number of digits this number type could take up.
 *
 * UINT8_MAX  255 - 3
 * UINT16_MAX 65535 - 5
 * UINT32_MAX 4294967295 - 10
 * UINT64_MAX 18446744073709551615 - 20
 *
 * The default case INT32_MIN is chosen so that static array allocations fail.
 */
#define MAXIMUM_DIGITS(number_type) ( \
        sizeof(number_type) == 1 ? 3 : \
        sizeof(number_type) == 2 ? 5 : \
        sizeof(number_type) == 4 ? 10 : \
        sizeof(number_type) == 8 ? 20 : INT32_MIN)

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

/* Duplicate a memory block. */
#define DUPLICATE(p, n) (xmemdup((p), sizeof(*(p)) * (n)))

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
    int x;
    /* vertical position */
    int y;
} Point;

/* a size of width x height */
typedef struct size {
    /* horizontal size */
    unsigned int width;
    /* vertical size */
    unsigned int height;
} Size;

/* offsets from the edges of *something* */
typedef struct extents {
    /* left extent */
    int left;
    /* right extent */
    int right;
    /* top extent */
    int top;
    /* bottom extent */
    int bottom;
} Extents;

/* a rectangular region */
typedef struct rectangle {
    /* horizontal position */
    int x;
    /* vertical position */
    int y;
    /* horizontal size */
    unsigned int width;
    /* vertical size */
    unsigned int height;
} Rectangle;

/* fraction: numerator over denominator */
typedef struct ratio {
    unsigned int numerator;
    unsigned int denominator;
} Ratio;

/* Run @command within a shell in the background. */
void run_shell(const char *command);

/* Run @command as new process and get the first line from it. */
char *run_command_and_get_output(const char *command);

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

/* Use the FNV-1 hash algorithm to compute the hash of @name. */
uint32_t get_fnv1_hash(const char *name);

#endif
