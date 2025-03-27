#include <string.h>

#include "parser.h"
#include "log.h"

/* information about instructions */
struct instruction_information {
    /* name of the instructions */
    const char *name;
    /* precedence of the instruction */
    enum precedence_class precedence;
} instruction_information[] = {
#define X(string, identifier, precedence) \
    [identifier] = { string, precedence },
    DEFINE_ALL_INSTRUCTIONS
#undef X
};

/* Get the name of an instruction. */
inline const char *instruction_type_to_string(instruction_type_t type)
{
    return instruction_information[type].name;
}

/* Get the precedence of an instruction. */
inline enum precedence_class get_instruction_precedence(instruction_type_t type)
{
    return instruction_information[type].precedence;
}

/* Parse an expression recursively. */
static parser_error_t parse_expression_recursively(Parser *parser,
        enum precedence_class precedence);

/* Insert an instruction to the instruction list. */
static inline void insert_instruction(Parser *parser,
        uint32_t position, uint32_t instruction)
{
    if (parser->instruction_size >= parser->instruction_capacity) {
        parser->instruction_capacity *= 2;
        RESIZE(parser->instructions, parser->instruction_capacity);
    }
    memmove(&parser->instructions[position + 1],
            &parser->instructions[position],
            sizeof(*parser->instructions) *
                (parser->instruction_size - position));
    parser->instruction_size++;
    parser->instructions[position] = instruction;
}

/* Append an instruction to the instruction list. */
static inline void push_instruction(Parser *parser, uint32_t instruction)
{
    insert_instruction(parser, parser->instruction_size, instruction);
}

/* Skip space and any new lines.
 *
 * @return if there is any new non empty lines.
 */
static inline bool skip_space_and_new_lines(Parser *parser)
{
    bool has_new_line = false;

    while (skip_space(parser), parser->line[parser->column] == '\0') {
        if (!read_next_line(parser)) {
            return false;
        }
        has_new_line = true;
    }
    return has_new_line;
}

/* Parse the next value within @parser.
 *
 * The integer may be of many forms:
 * #... (hexadecimal digits)
 * [0-9]+
 * boolean constant
 * modifier constant
 * cursor constant
 */
static inline parser_error_t parse_integer_value(Parser *parser)
{
    parser_error_t error;
    int32_t integer;

    if (parser->line[parser->column] == '#') {
        parser->column++;
        /* interpret the digits as hexadecimal */
        for (integer = 0;
                isxdigit(parser->line[parser->column]);
                parser->column++) {
            integer <<= 4;
            integer |= isdigit(parser->line[parser->column]) ?
                parser->line[parser->column] - '0' :
                tolower(parser->line[parser->column]) - 'a' + 0xa;
            /* allow overflow */
        }
    } else if (isdigit(parser->line[parser->column])) {
        /* read all digits while not overflowing */
        for (integer = 0;
                isdigit(parser->line[parser->column]);
                parser->column++) {
            integer *= 10;
            integer += parser->line[parser->column] - '0';
            if (integer > PARSER_INTEGER_LIMIT) {
                integer = PARSER_INTEGER_LIMIT;
            }
        }
    } else {
        error = parse_identifier(parser);
        if (error != PARSER_SUCCESS) {
            return error;
        }
        /* try to resolve the identifier constant using various methos */
        do {
            bool boolean;
            uint16_t modifier;

            if (string_to_boolean(parser->identifier_lower, &boolean) == OK) {
                integer = boolean;
                break;
            }

            if (string_to_modifier(parser->identifier_lower, &modifier) == OK) {
                integer = modifier;
                break;
            }

            /* translate - to _ */
            for (uint32_t i = 0; parser->identifier[i] != '\0'; i++) {
                if (parser->identifier[i] == '-') {
                    parser->identifier[i] = '_';
                }
            }
            integer = string_to_cursor(parser->identifier);
            if ((core_cursor_t) integer != XCURSOR_MAX) {
                break;
            }
            /* we do not translate back because only `identifier_lower` will be
             * used next when parsing an action
             */
            return PARSER_UNEXPECTED;
        } while (false);
    }

    push_instruction(parser, MAKE_INTEGER(integer));
    return PARSER_SUCCESS;
}

/* Parse an action identifier and its parameter.
 *
 * An identifier must have been loaded into @parser.
 */
static inline parser_error_t parse_action(Parser *parser)
{
    parser_error_t error = PARSER_SUCCESS;
    action_type_t type;
    uint32_t position;

    type = string_to_action_type(parser->identifier_lower);
    if (type == ACTION_NULL) {
        return PARSER_ERROR_INVALID_ACTION;
    }

    position = parser->instruction_size;

    push_instruction(parser, MAKE_ACTION(type));

    const data_type_t data_type = get_action_data_type(type);
    switch (data_type) {
    /* no data type expected */
    case DATA_TYPE_VOID:
        parser->instructions[position] = MAKE_VOID_ACTION(type);
        break;

    /* utf8 byte sequence */
    case DATA_TYPE_STRING: {
        utf8_t *string;
        uint32_t length;
        uint32_t length_in_4bytes;
        uint32_t new_size;

        error = parse_string(parser, &string);
        if (error == PARSER_UNEXPECTED && has_action_optional_argument(type)) {
            error = PARSER_SUCCESS;
            parser->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }
        if (error != PARSER_SUCCESS) {
            break;
        }

        push_instruction(parser, 0);

        length = strlen((char*) string);
        length_in_4bytes = length / sizeof(*parser->instructions) + 1;
        new_size = parser->instruction_size + length_in_4bytes;
        if (new_size > parser->instruction_capacity) {
            parser->instruction_capacity += new_size;
            RESIZE(parser->instructions, parser->instruction_capacity);
        }
        memcpy(&parser->instructions[parser->instruction_size],
                string, length + 1);

        free(string);

        parser->instruction_size = new_size;

        parser->instructions[position + 1] = MAKE_STRING(length_in_4bytes);
        break;
    }

    /* 1, 2 or 4 integer expressions */
    case DATA_TYPE_QUAD:
        error = parse_quad_expression(parser);
        if (error == PARSER_UNEXPECTED && has_action_optional_argument(type)) {
            error = PARSER_SUCCESS;
            parser->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }
        if (error != PARSER_SUCCESS) {
            break;
        }
        break;

    /* integer expression */
    case DATA_TYPE_INTEGER:
        error = parse_expression_recursively(parser, PRECEDENCE_ACTION);
        if (error == PARSER_UNEXPECTED && has_action_optional_argument(type)) {
            error = PARSER_SUCCESS;
            parser->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }
        break;

    default:
        error = PARSER_ERROR_UNEXPECTED;
        break;
    }

    return error;
}

/* Parse an expression. */
static parser_error_t parse_expression_recursively(Parser *parser,
        enum precedence_class precedence)
{
    parser_error_t error;
    uint32_t instruction = 0;
    enum precedence_class other_precedence = 0;
    uint32_t position;

    skip_space(parser);

    switch (parser->line[parser->column]) {
    case '\0':
        return PARSER_UNEXPECTED;

    /* prefix operators */
    case '!':
        parser->column++;
        instruction = INSTRUCTION_NOT;
        other_precedence = PRECEDENCE_NOT;
        break;
    case '+':
        parser->column++;
        other_precedence = PRECEDENCE_NEGATE;
        break;
    case '-':
        parser->column++;
        instruction = INSTRUCTION_NEGATE;
        other_precedence = PRECEDENCE_NEGATE;
        break;

    /* opening bracket that allows to group instructions and operations together
     */
    case '(':
        parser->column++;
        other_precedence = PRECEDENCE_OPEN_BRACKET;
        break;
    }

    position = parser->instruction_size;

    if (instruction > 0) {
        push_instruction(parser, instruction);
    }

    if (other_precedence > 0) {
        (void) skip_space_and_new_lines(parser);
        error = parse_expression_recursively(parser, other_precedence);
        if (error != PARSER_SUCCESS) {
            return error;
        }
    } else {
        error = parse_integer_value(parser);
        switch (error) {
        case PARSER_SUCCESS:
            /* do nothing */
            break;

        case PARSER_UNEXPECTED:
            error = parse_action(parser);
            if (error != PARSER_SUCCESS) {
                return error;
            }
            break;

        default:
            return error;
        }
    }

    while (1) {
        instruction = 0;

        skip_space(parser);

        switch (parser->line[parser->column]) {
        case '\0':
            if (precedence == PRECEDENCE_OPEN_BRACKET) {
                /* if a bracket is open, we look on the following lines for a
                 * closing bracket
                 */
                if (skip_space_and_new_lines(parser)) {
                    /* check if there is an operator on the next line */
                    if (strchr("&|)", parser->line[parser->column]) != NULL) {
                        continue;
                    }
                    /* implicit ; */
                    insert_instruction(parser, position, INSTRUCTION_NEXT);
                    error = parse_expression_recursively(parser,
                            PRECEDENCE_SEMICOLON);
                    if (error != PARSER_SUCCESS) {
                        return error;
                    }
                    continue;
                } else {
                    return PARSER_ERROR_MISSING_CLOSING_BRACKET;
                }
            }
            return PARSER_SUCCESS;

        /* handle || and && */
        case '|':
        case '&': {
            uint32_t jump_offset;

            const char operator = parser->line[parser->column];
            other_precedence = operator == '&' ?  PRECEDENCE_LOGICAL_AND :
                PRECEDENCE_LOGICAL_OR;

            if (precedence > other_precedence) {
                return PARSER_SUCCESS;
            }
            parser->column++;
            if (parser->line[parser->column] != operator) {
                return PARSER_ERROR_UNEXPECTED;
            }
            parser->column++;
            jump_offset = parser->instruction_size;
            insert_instruction(parser, position, operator == '&' ?
                    INSTRUCTION_LOGICAL_AND : INSTRUCTION_LOGICAL_OR);
            (void) skip_space_and_new_lines(parser);
            error = parse_expression_recursively(parser, other_precedence);
            if (error != PARSER_SUCCESS) {
                return error;
            }
            jump_offset = parser->instruction_size - jump_offset - 1;
            parser->instructions[position] |= jump_offset << 8;
            break;
        }

        /* separator operator */
        case ';':
            other_precedence = PRECEDENCE_SEMICOLON;
            instruction = INSTRUCTION_NEXT;
            break;

        /* operators with PRECEDENCE_PLUS precedence */
        case '+':
            other_precedence = PRECEDENCE_PLUS;
            instruction = INSTRUCTION_ADD;
            break;
        case '-':
            other_precedence = PRECEDENCE_PLUS;
            instruction = INSTRUCTION_SUBTRACT;
            break;

        /* operators with PRECEDENCE_MULTIPLY precedence */
        case '*':
            other_precedence = PRECEDENCE_MULTIPLY;
            instruction = INSTRUCTION_MULTIPLY;
            break;
        case '/':
            other_precedence = PRECEDENCE_MULTIPLY;
            instruction = INSTRUCTION_DIVIDE;
            break;
        case '%':
            other_precedence = PRECEDENCE_MULTIPLY;
            instruction = INSTRUCTION_MODULO;
            break;

        /* a closing bracket */
        case ')':
            if (precedence < PRECEDENCE_OPEN_BRACKET) {
                return PARSER_ERROR_MISSING_OPENING_BRACKET;
            }
            /* let the higher function take care of the closing bracket */
            if (precedence > PRECEDENCE_OPEN_BRACKET) {
                return PARSER_SUCCESS;
            }
            parser->column++;
            return PARSER_SUCCESS;

        default:
            return PARSER_UNEXPECTED;
        }

        if (instruction > 0) {
            /* if the precedence is higher, return and let the higher function
             * take care of this operator
             */
            if (precedence > other_precedence) {
                return PARSER_SUCCESS;
            }
            parser->column++;
            insert_instruction(parser, position, instruction);
            (void) skip_space_and_new_lines(parser);
            error = parse_expression_recursively(parser, other_precedence);
            if (error != PARSER_SUCCESS) {
                return error;
            }
        }
    }
}

/* Parse an expression. */
parser_error_t parse_expression(Parser *parser)
{
    parser->instruction_size = 0;
    return parse_expression_recursively(parser, PRECEDENCE_ORIGIN);
}

/* Parse 1, 2 or 4 four expressions in series. */
parser_error_t parse_quad_expression(Parser *parser)
{
    parser_error_t error;
    uint32_t position;
    uint32_t count = 0;

    position = parser->instruction_size;

    push_instruction(parser, 0);

    /* get the quad arguments up to 4 */
    while (count < 4) {
        count++;
        error = parse_expression_recursively(parser, PRECEDENCE_ACTION);
        /* if the end was reached, prematurely end */
        if (error == PARSER_SUCCESS) {
            break;
        }
        /* if an actual error happened, break */
        if (error != PARSER_UNEXPECTED) {
            break;
        }
        /* PARSER_UNEXPECTED is fine */
        error = PARSER_SUCCESS;
    }

    if (error == PARSER_UNEXPECTED && count != 0) {
        error = PARSER_SUCCESS;
    }

    if (error != PARSER_SUCCESS) {
        return error;
    }

    if (count == 3 || count == 0) {
        return PARSER_ERROR_INVALID_QUAD;
    }

    parser->instructions[position] = MAKE_QUAD(count);

    return PARSER_SUCCESS;
}

/* Allocate an expression from previously parsed expressions. */
void extract_expression(Parser *parser, Expression *expression)
{
    expression->instructions = DUPLICATE(
            parser->instructions,
            parser->instruction_size);
    expression->instruction_size = parser->instruction_size;
}
