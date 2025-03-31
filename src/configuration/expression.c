#include <string.h>

#include "configuration/expression.h"
#include "configuration/instructions.h"
#include "configuration/stack.h"
#include "utility.h"

/* Copy @instructions and @stack into @expression. */
void initialize_expression(Expression *expression, const uint32_t *instructions,
        uint32_t instruction_size)
{
    expression->instructions = DUPLICATE(instructions, instruction_size);
    expression->instruction_size = instruction_size;
}

/* Helper function to make an expression for running a simple action. */
void initialize_expression_from_action(Expression *expression,
        action_type_t type, GenericData *data)
{
    if (data == NULL) {
        expression->instruction_size = 1;
        expression->instructions = xmalloc(sizeof(*expression->instructions) *
            expression->instruction_size);
        expression->instructions[0] = MAKE_INSTRUCTION(type,
                INSTRUCTION_RUN_VOID_ACTION);
        return;
    }

    const data_type_t data_type = get_action_data_type(type);
    switch (data_type) {
    case DATA_TYPE_VOID:
        expression->instruction_size = 1;
        expression->instructions = xmalloc(sizeof(*expression->instructions) *
            expression->instruction_size);
        expression->instructions[0] = MAKE_INSTRUCTION(type,
                INSTRUCTION_RUN_VOID_ACTION);
        break;

    case DATA_TYPE_INTEGER:
        expression->instruction_size = 2;
        expression->instructions = xmalloc(sizeof(*expression->instructions) *
                expression->instruction_size);
        expression->instructions[0] = MAKE_INSTRUCTION(type,
                INSTRUCTION_RUN_ACTION);
        expression->instructions[1] = MAKE_INSTRUCTION(data->integer,
                LITERAL_INTEGER);
        break;

    case DATA_TYPE_QUAD:
        expression->instruction_size = 6;
        expression->instructions = xmalloc(sizeof(*expression->instructions) *
                expression->instruction_size);
        expression->instructions[0] = MAKE_INSTRUCTION(type,
                INSTRUCTION_RUN_ACTION);
        expression->instructions[1] = MAKE_INSTRUCTION(4, LITERAL_QUAD);
        for (int i = 0; i < 4; i++) {
            expression->instructions[i + 2] = MAKE_INSTRUCTION(data->quad[i],
                    LITERAL_INTEGER);
        }
        break;

    case DATA_TYPE_STRING: {
        uint32_t length;
        uint32_t length_in_4bytes;

        length = strlen((char*) data->string);
        length_in_4bytes = length / sizeof(*expression->instructions) + 1;
        expression->instruction_size = 2 + length_in_4bytes;
        expression->instructions = xmalloc(sizeof(*expression->instructions) *
                expression->instruction_size);
        expression->instructions[0] = MAKE_INSTRUCTION(type,
                INSTRUCTION_RUN_ACTION);
        expression->instructions[1] = MAKE_INSTRUCTION(length_in_4bytes,
                LITERAL_STRING);
        memcpy(&expression->instructions[2], data->string, length + 1);
        break;
    }

    default:
        /* TODO: implement more if more is needed */
        break;
    }
}


/* Evaluate given expression. */
void evaluate_expression(const Expression *expression, GenericData *result)
{
    const uint32_t *instructions, *end;
    GenericData alternative_result;

    if (result == NULL) {
        result = &alternative_result;
    }

    instructions = expression->instructions;
    end = &expression->instructions[expression->instruction_size];

    /* run all instructions in series */
    while (instructions < end) {
        instructions = run_instruction(instructions, result);
    }
}
