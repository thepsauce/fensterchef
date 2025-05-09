#ifndef UTILITY__TYPES_H
#define UTILITY__TYPES_H

/**
 * Helpful data types.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* byte of a utf8 sequence */
typedef char utf8_t;

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

#endif
