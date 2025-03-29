#ifndef CONFIGURATION__INSTRUCTIONS_H
#define CONFIGURATION__INSTRUCTIONS_H

#include "configuration/data_type.h"

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
    /* = */
    PRECEDENCE_SET,
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
    /* a variable */ \
    X("variable", INSTRUCTION_VARIABLE, PRECEDENCE_LITERAL) \
\
    X("next", INSTRUCTION_NEXT, PRECEDENCE_SEMICOLON) \
\
    /* only execute the second instruction if the first succeeded */ \
    X("logical_and", INSTRUCTION_LOGICAL_AND, PRECEDENCE_LOGICAL_AND) \
    /* only execute the second instruction if the first failed */ \
    X("logical_or", INSTRUCTION_LOGICAL_OR, PRECEDENCE_LOGICAL_OR) \
\
    /* setting of a variable */ \
    X("set", INSTRUCTION_SET, PRECEDENCE_SET) \
    /* push an integer onto the stack */ \
    X("push-integer", INSTRUCTION_PUSH_INTEGER, PRECEDENCE_SET) \
    /* load an integer from the stack */ \
    X("load-integer", INSTRUCTION_LOAD_INTEGER, PRECEDENCE_LITERAL) \
    /* set an integer on the stack */ \
    X("set-integer", INSTRUCTION_SET_INTEGER, PRECEDENCE_SET) \
    /* set the stack pointer */ \
    X("stack-pointer", INSTRUCTION_STACK_POINTER, PRECEDENCE_LITERAL) \
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
    X("void-action", INSTRUCTION_RUN_VOID_ACTION, PRECEDENCE_ACTION) \

#define MAKE_INSTRUCTION(high, low) \
    (((uint32_t) (high) << 8) | low)

/* an instruction type */
typedef enum instruction_type {
#define X(string, identifier, precedence) \
    identifier,
    DEFINE_ALL_INSTRUCTIONS
#undef X
} instruction_type_t;

/* Get the name of an instruction. */
const char *instruction_type_to_string(instruction_type_t type);

/* Get the precedence of an instruction. */
enum precedence_class get_instruction_precedence(instruction_type_t type);

/* Run the next instruction within @instructions.
 *
 * @data is used to store any results.
 *
 * @return the pointer to the instruction after @instructions.
 */
const uint32_t *run_instruction(const uint32_t *instructions,
        GenericData *data);

#endif
