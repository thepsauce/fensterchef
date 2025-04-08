#include <string.h>

#include "configuration/stack.h"

#include "utility.h"

/* the stack is used to hold onto temporary values */
struct stack stack;

/* Set the stack to something else. */
void set_stack(const uint32_t *values, uint32_t size)
{
    if (stack.capacity < size) {
        stack.capacity = size;
        REALLOCATE(stack.values, size);
    }
    stack.size = size;
    memcpy(stack.values, values, sizeof(*values) * size);
}


/* Push an integer onto the stack. */
void push_integer(int32_t integer)
{
    if (stack.capacity == 0) {
        stack.capacity = 4;
        REALLOCATE(stack.values, stack.capacity);
    } else if (stack.size == stack.capacity) {
        stack.capacity *= 2;
        REALLOCATE(stack.values, stack.capacity);
    }
    stack.values[stack.size] = integer;
    stack.size++;
}
