#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdint.h>

#include "data_type.h"

/* precedence classes */
enum precedence_class {
    /* the base precedence */
    PRECEDENCE_ORIGIN,
    /* ( */
    PRECEDENCE_OPEN_BRACKET,
    /* ; */
    PRECEDENCE_SEMICOLON,
    /* || */
    PRECEDENCE_LOGICAL_OR,
    /* && */
    PRECEDENCE_LOGICAL_AND,
    /* ACTION_* */
    PRECEDENCE_ACTION,
    /* + - */
    PRECEDENCE_PLUS,
    /* + - (as prefix operator) */
    PRECEDENCE_NEGATE,
    /* * / % */
    PRECEDENCE_MULTIPLY,
    /* ! */
    PRECEDENCE_NOT,
    /* . */
    /* TODO: use me */
    PRECEDENCE_DOT,
    /* literal type */
    PRECEDENCE_LITERAL,
};

#define DEFINE_ALL_INSTRUCTIONS \
    /* 24 bit signed integer data type */ \
    X("integer", LITERAL_INTEGER, PRECEDENCE_LITERAL) \
    /* utf8 encoded string padded to a 4 byte boundary */ \
    X("string", LITERAL_STRING, PRECEDENCE_LITERAL) \
    /* a set of 1, 2 or 4 integers */ \
    X("quad", LITERAL_QUAD, PRECEDENCE_LITERAL) \
\
    X("next", INSTRUCTION_NEXT, PRECEDENCE_SEMICOLON) \
\
    /* only execute the second instruction if the first succeeded */ \
    X("logical_and", INSTRUCTION_LOGICAL_AND, PRECEDENCE_LOGICAL_AND) \
    /* only execute the second instruction if the first failed */ \
    X("logical_or", INSTRUCTION_LOGICAL_OR, PRECEDENCE_LOGICAL_OR) \
\
    /* invert the truthness of an integer */ \
    X("not", INSTRUCTION_NOT, PRECEDENCE_NOT) \
    /* negate an integer */ \
    X("negate", INSTRUCTION_NEGATE, PRECEDENCE_NEGATE) \
    /* add two integers */ \
    X("add", INSTRUCTION_ADD, PRECEDENCE_PLUS) \
    /* subtract two integers */ \
    X("subtract", INSTRUCTION_SUBTRACT, PRECEDENCE_PLUS) \
    /* multiply two integers */ \
    X("multiply", INSTRUCTION_MULTIPLY, PRECEDENCE_MULTIPLY) \
    /* divide two integers */ \
    X("divide", INSTRUCTION_DIVIDE, PRECEDENCE_MULTIPLY) \
    /* take the modulos of two integers */ \
    X("modulo", INSTRUCTION_MODULO, PRECEDENCE_MULTIPLY) \
\
    /* run a specific action */ \
    X("action", INSTRUCTION_RUN_ACTION, PRECEDENCE_ACTION) \
    /* run a specific action without parameter */ \
    X("void-action", INSTRUCTION_RUN_VOID_ACTION, PRECEDENCE_ACTION)

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
#define X(string, identifier, precedence) \
    identifier,
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

/* Get the name of an instruction. */
const char *instruction_type_to_string(instruction_type_t type);

/* Get the precedence of an instruction. */
enum precedence_class get_instruction_precedence(instruction_type_t type);

/* Evaluate given expression.
 *
 * @result may be NULL.
 */
void evaluate_expression(const Expression *expression, GenericData *result);

#endif
