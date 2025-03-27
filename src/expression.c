#include <string.h>

#include "action.h"
#include "expression.h"
#include "utility.h"

#include "log.h"

/* Get the data type of an instruction. */
data_type_t get_instruction_data_type(uint32_t type)
{
    switch (type) {
    case LITERAL_STRING:
        return DATA_TYPE_STRING;

    default:
        return DATA_TYPE_INTEGER;
    }
}

/* Run given instruction.
 *
 * @data is used to store any results.
 *
 * @return the pointer to the instruction after @instructions.
 */
const uint32_t *run_instruction(const uint32_t *instructions, GenericData *data)
{
    const uint32_t instruction = instructions[0];
    instructions++;
    const instruction_type_t type = (instruction & 0xff);
    LOG_VERBOSE("executing instruction: %u (%06x)\n", type, instruction >> 8);
    switch (type) {
    /* get a 24 bit signed integer */
    case LITERAL_INTEGER: {
        struct {
            int32_t x : 24;
        } sign_extend;
        sign_extend.x = instruction >> 8;
        data->integer = sign_extend.x;
        LOG_VERBOSE("have literal integer: %d\n", data->integer);
        break;
    }

    /* a quad value */
    case LITERAL_QUAD: {
        switch (instruction >> 8) {
        /* copy the single value into the other */
        case 1:
            instructions = run_instruction(instructions, data);
            data->quad[1] = data->quad[0];
            data->quad[2] = data->quad[0];
            data->quad[3] = data->quad[0];
            break;

        /* copy the two values into the other */
        case 2:
            instructions = run_instruction(instructions, data);
            instructions = run_instruction(instructions,
                    (GenericData*) &data->quad[1]);
            data->quad[2] = data->quad[0];
            data->quad[3] = data->quad[1];
            break;

        /* simply get all four values */
        case 4:
            instructions = run_instruction(instructions, data);
            instructions = run_instruction(instructions,
                    (GenericData*) &data->quad[1]);
            instructions = run_instruction(instructions,
                    (GenericData*) &data->quad[2]);
            instructions = run_instruction(instructions,
                    (GenericData*) &data->quad[3]);
            break;
        }
        break;
    }

    /* get a string */
    case LITERAL_STRING:
        data->string = (utf8_t*) &instructions[0];
        instructions += instruction >> 8;
        LOG_VERBOSE("have literal string: %s\n", data->string);
        break;

    /* execute the two next instructions */
    case INSTRUCTION_NEXT:
        instructions = run_instruction(instructions, data);
        instructions = run_instruction(instructions, data);
        break;

#define JUMP_OPERATION(compare) do { \
    instructions = run_instruction(instructions, data); \
    if (data->integer compare 0) { \
        instructions = run_instruction(instructions, data); \
    } else { \
        instructions += instruction >> 8; \
    } \
} while (false)

    /* jump operations */
    case INSTRUCTION_LOGICAL_AND:
        JUMP_OPERATION(!=);
        break;
    case INSTRUCTION_LOGICAL_OR:
        JUMP_OPERATION(==);
        break;

#undef JUMP_OPERATION

#define INTEGER_OPERATION(operator) do { \
    int32_t second; \
\
    instructions = run_instruction(instructions, data); \
    instructions = run_instruction(instructions, (GenericData*) &second); \
    data->integer operator##= second; \
    LOG_VERBOSE("result of operation: %d\n", data->integer); \
} while (false)

    /* integer operations */
    case INSTRUCTION_NEGATE:
        instructions = run_instruction(instructions, data);
        data->integer *= -1;
        LOG_VERBOSE("result of negation: %d\n", data->integer);
        break;
    case INSTRUCTION_ADD:
        INTEGER_OPERATION(+);
        break;
    case INSTRUCTION_SUBTRACT:
        INTEGER_OPERATION(-);
        break;
    case INSTRUCTION_MULTIPLY:
        INTEGER_OPERATION(*);
        break;
    case INSTRUCTION_DIVIDE:
        INTEGER_OPERATION(/);
        break;
    case INSTRUCTION_MODULO:
        INTEGER_OPERATION(%);
        break;

#undef INTEGER_OPERATION

    /* run an action */
    case INSTRUCTION_RUN_ACTION:
        instructions = run_instruction(instructions, data);
        data->integer = !do_action(instruction >> 8, data);
        break;

    /* run an action without parameter */
    case INSTRUCTION_RUN_VOID_ACTION:
        data->integer = !do_action(instruction >> 8, NULL);
        break;
    }
    return instructions;
}

/* Evaluate given expression. */
int evaluate_expression(const Expression *expression)
{
    const uint32_t *instructions, *end;
    GenericData data;

    instructions = expression->instructions;
    end = &expression->instructions[expression->instruction_size];

    while (instructions < end) {
        instructions = run_instruction(instructions, &data);
        if (instructions == NULL) {
            return ERROR;
        }
    }
    return OK;
}
