#ifndef CONFIGURATION__STACK_H
#define CONFIGURATION__STACK_H

#include <stdint.h>

/* the stack is used to hold onto temporary values */
extern struct stack {
    /* the value on the stack */
    uint32_t *values;
    /* the stack pointer */
    uint32_t size;
    /* how many values are allocated on the stack */
    uint32_t capacity;
} stack;

/* Set the stack to something else. */
void set_stack(const uint32_t *values, uint32_t size);

/* Push an integer onto the stack. */
void push_integer(int32_t integer);

#endif
