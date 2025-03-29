#ifndef CONFIGURATION__EXPRESSION_H
#define CONFIGURATION__EXPRESSION_H

#include <stdint.h>

#include "action.h"
#include "data_type.h"

/* a generic expression */
typedef struct expression {
    /* there is always a preceding 32 bit instruction identifier and then more
     * instructions or a value.
     * The lower 8 bits is an `instruction_type_t`.
     * The higher 24 bits contain special data specific to the instruction.
     */
    uint32_t *instructions;
    /* the size of the the instruction list in 4 byte units */
    uint32_t instruction_size;
    /* the stack is used to hold onto temporary values */
    uint32_t *stack;
    /* the number of items in `stack` */
    uint32_t stack_size;
} Expression;

/* Helper function to make an expression for running a simple action. */
void initialize_expression_from_action(Expression *expression,
        action_type_t type, GenericData *data);

/* Evaluate given expression.
 *
 * @result may be NULL.
 */
void evaluate_expression(const Expression *expression, GenericData *result);

#endif
