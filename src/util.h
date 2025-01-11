#ifndef UTIL_H
#define UTIL_H

#include "xalloc.h"

#define ISIDENTF(c) (isalpha(c)||(c)=='_')
#define SIZE(a) (sizeof(a)/sizeof(*(a)))
#define RESIZE(p, a) \
    ((p) = xreallocarray(p, (a), sizeof(*p)));
#define GROWBY(p, n, a, i) do { \
    if ((n) + (i) > (a)) { \
        (a) *= 2; \
        (a) += (i); \
        RESIZE(p, a); \
    } \
} while (0)
#define GROWTO(p, a, at) do { \
    if ((at) > (a)) { \
        (a) = (at); \
        RESIZE(p, a); \
    } \
} while (0)
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
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

#define OVERFLOW_MUL(a, b, c) \
    __builtin_mul_overflow((a), (b), &(c))

#define OVERFLOW_ADD(a, b, c) \
    __builtin_add_overflow((a), (b), &(c))

#endif
