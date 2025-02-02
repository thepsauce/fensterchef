#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#include "xalloc.h"

/* Check if given character is the start of an identifier. */
#define ISIDENTF(c) (isalpha(c)||(c)=='_')

/* Get the size of a statically sized array. */
#define SIZE(a) (sizeof(a)/sizeof(*(a)))

/* Resize allocated memory to given size. */
#define RESIZE(p, a) \
    ((p) = xreallocarray(p, (a), sizeof(*p)));

/* Grow allocated memory by given growth increment @i. */
#define GROWBY(p, n, a, i) do { \
    if ((n) + (i) > (a)) { \
        (a) *= 2; \
        (a) += (i); \
        RESIZE(p, a); \
    } \
} while (0)

/* Grow allocated memory to given size. */
#define GROWTO(p, a, at) do { \
    if ((at) > (a)) { \
        (a) = (at); \
        RESIZE(p, a); \
    } \
} while (0)

/* Get the maximum of two numbers. */
#define MAX(a, b) ((a)>(b)?(a):(b))

/* Get the minimum of two numbers. */
#define MIN(a, b) ((a)<(b)?(a):(b))

/* Get the absolute difference of tow numbers. */
#define ABS_DIFF(a, b) (MAX(a,b)-MIN(a,b))

/* a = MIN(a * b, c) */
#define CLIP_MUL(a, b, c) do { \
    int __c; \
    if (__builtin_mul_overflow((a), (b), &__c)) { \
        (a) = (c); \
    } else { \
        (a) = MIN(__c, c); \
    } \
} while (0)

/* Check if the multiplication overflows and store the result in @c. */
#define OVERFLOW_MUL(a, b, c) \
    __builtin_mul_overflow((a), (b), &(c))

/* Check if the addition overflows and store the result in @c. */
#define OVERFLOW_ADD(a, b, c) \
    __builtin_add_overflow((a), (b), &(c))

typedef struct position {
    int32_t x;
    int32_t y;
} Position;

typedef struct size {
    uint32_t width;
    uint32_t height;
} Size;

#endif
