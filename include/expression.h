#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdint.h>

#include "data_type.h"

#define DEFINE_ALL_INSTRUCTIONS \
    /* 24 bit signed integer data type */ \
    X("integer", LITERAL_INTEGER) \
    /* utf8 encoded string padded to a 4 byte boundary */ \
    X("string", LITERAL_STRING) \
    /* a set of 1, 2 or 4 integers */ \
    X("quad", LITERAL_QUAD) \
\
    X("next", INSTRUCTION_NEXT) \
\
    /* only execute the second instruction if the first succeeded */ \
    X("logical_and", INSTRUCTION_LOGICAL_AND) \
    /* only execute the second instruction if the first failed */ \
    X("logical_or", INSTRUCTION_LOGICAL_OR) \
\
    /* negate an integer */ \
    X("negate", INSTRUCTION_NEGATE) \
    /* add two integers */ \
    X("add", INSTRUCTION_ADD) \
    /* subtract two integers */ \
    X("subtract", INSTRUCTION_SUBTRACT) \
    /* multiply two integers */ \
    X("multiply", INSTRUCTION_MULTIPLY) \
    /* divide two integers */ \
    X("divide", INSTRUCTION_DIVIDE) \
    /* take the modulos of two integers */ \
    X("modulo", INSTRUCTION_MODULO) \
\
    /* run a specific action */ \
    X("action", INSTRUCTION_RUN_ACTION) \
    /* run a specific action without parameter */ \
    X("void-action", INSTRUCTION_RUN_VOID_ACTION)

/* Make an integer instruction. */
#define MAKE_INTEGER(integer) \
    ((uint32_t) (integer) << 8) | LITERAL_INTEGER

/* Make a string instruction. */
#define MAKE_STRING(length) \
    ((uint32_t) (length) << 8) | LITERAL_STRING

/* Make a quad instruction. */
#define MAKE_QUAD(count) \
    ((uint32_t) (count) << 8) | LITERAL_QUAD

/* Make a run action instruction. */
#define MAKE_ACTION(type) \
    ((action_type_t) (type) << 8) | INSTRUCTION_RUN_ACTION

/* Make a run void action instruction. */
#define MAKE_VOID_ACTION(type) \
    ((action_type_t) (type) << 8) | INSTRUCTION_RUN_VOID_ACTION

/* an instruction type */
typedef enum instruction_type {
#define X(string, identifier) identifier,
    DEFINE_ALL_INSTRUCTIONS
#undef X
} instruction_type_t;

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
} Expression;

/* Evaluate given expression. */
int evaluate_expression(const Expression *expression);

#endif
