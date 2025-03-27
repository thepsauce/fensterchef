#include <string.h>

#include "configuration_parser.h"

/* precedence classes */
enum precedence_class {
    /* the base precedence */
    ORIGIN,
    /* ( */
    OPEN_BRACKET,
    /* ; */
    SEMICOLON,
    /* && || */
    LOGICAL_AND,
    /* ACTION_* */
    ACTION,
    /* + - */
    PLUS,
    /* * / % */
    MULTIPLY,
    /* . */
    DOT,
    /* literal type */
    LITERAL,
    /* placeholder precedence */
    PLACEHOLDER,
};

/* helper struct */
struct helper {
    /* the last error that happened */
    parser_error_t error;
    /* the instructions being parsed to */
    uint32_t *instructions;
    /* number of instructions */
    uint32_t size;
    /* number of allocated instructions */
    uint32_t capacity;
};

/* Insert an instruction to the instruction list. */
static inline void insert_instruction(struct helper *helper, uint32_t position,
        uint32_t instruction)
{
    if (helper->size >= helper->capacity) {
        helper->capacity *= 2;
        RESIZE(helper->instructions, helper->capacity);
    }
    memmove(&helper->instructions[position + 1],
            &helper->instructions[position],
            sizeof(*helper->instructions) * (helper->size - position));
    helper->size++;
    helper->instructions[position] = instruction;
}

/* Append an instruction to the instruction list. */
static inline void push_instruction(struct helper *helper, uint32_t instruction)
{
    insert_instruction(helper, helper->size, instruction);
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
 * @return false if there is none or if there was an error.
 */
static inline bool parse_integer_value(Parser *parser, struct helper *helper)
{
    parser_error_t error;
    int32_t integer;

    if (isdigit(parser->line[parser->column])) {
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
            helper->error = error;
            return false;
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
            return false;
        } while (false);
    }

    push_instruction(helper, MAKE_INTEGER(integer));
    return true;
}

/* Parse an expression. */
static parser_error_t parse_expression_recursively(Parser *parser,
        struct helper *helper, enum precedence_class precedence);

/* Parse an action identifier and its parameter. */
static inline parser_error_t parse_action(Parser *parser, struct helper *helper)
{
    parser_error_t error;
    action_type_t type;
    uint32_t position;

    type = string_to_action_type(parser->identifier_lower);
    if (type == ACTION_NULL) {
        return PARSER_ERROR_INVALID_ACTION;
    }

    position = helper->size;

    push_instruction(helper, MAKE_ACTION(type));

    const data_type_t data_type = get_action_data_type(type);
    switch (data_type) {
    /* no data type expected */
    case DATA_TYPE_VOID:
        helper->instructions[position] = MAKE_VOID_ACTION(type);
        break;

    /* utf8 byte sequence */
    case DATA_TYPE_STRING: {
        utf8_t *string;
        uint32_t length;
        uint32_t length_in_4bytes;
        uint32_t new_size;

        error = parse_string(parser, &string);
        if (error == PARSER_UNEXPECTED && has_action_optional_argument(type)) {
            helper->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }
        if (error != PARSER_SUCCESS) {
            return error;
        }

        push_instruction(helper, 0);

        length = strlen((char*) string);
        length_in_4bytes = length / sizeof(*helper->instructions) + 1;
        new_size = helper->size + length_in_4bytes;
        if (new_size > helper->capacity) {
            helper->capacity += new_size;
            RESIZE(helper->instructions, helper->capacity);
        }
        memcpy(&helper->instructions[helper->size], string, length + 1);

        free(string);

        helper->size = new_size;

        helper->instructions[position + 1] = MAKE_STRING(length_in_4bytes);
        break;
    }

    /* 1, 2 or 4 integer expressions */
    case DATA_TYPE_QUAD: {
        uint32_t count = 0;

        push_instruction(helper, 0);

        /* get the quad arguments up to 4 */
        while (count < 4) {
            count++;
            error = parse_expression_recursively(parser, helper, ACTION);
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

        if (error == PARSER_UNEXPECTED && count == 0 &&
                has_action_optional_argument(type)) {
            helper->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }

        if (error != PARSER_SUCCESS) {
            return error;
        }

        if (count == 3 || count == 0) {
            return PARSER_ERROR_INVALID_QUAD;
        }
        helper->instructions[position + 1] = MAKE_QUAD(count);
        break;
    }

    /* integer expression */
    case DATA_TYPE_INTEGER:
        error = parse_expression_recursively(parser, helper, ACTION);
        if (error == PARSER_UNEXPECTED && has_action_optional_argument(type)) {
            helper->instructions[position] = MAKE_VOID_ACTION(type);
            break;
        }
        return error;

    default:
        return PARSER_ERROR_UNEXPECTED;
    }

    return PARSER_SUCCESS;
}

/* Parse an expression. */
static parser_error_t parse_expression_recursively(Parser *parser,
        struct helper *helper, enum precedence_class precedence)
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
    case '+':
        parser->column++;
        other_precedence = PLUS + 1;
        break;
    case '-':
        parser->column++;
        instruction = INSTRUCTION_NEGATE;
        /* `+1` so that for "-1 - 2", the "-" does not wrap around "1 - 2" */
        other_precedence = PLUS + 1;
        break;

    /* opening bracket that allows to group instructions and operations together
     */
    case '(':
        parser->column++;
        other_precedence = OPEN_BRACKET;
        break;
    }

    position = helper->size;

    if (instruction > 0) {
        push_instruction(helper, instruction);
    }

    if (other_precedence > 0) {
        (void) skip_space_and_new_lines(parser);
        error = parse_expression_recursively(parser, helper, other_precedence);
        if (error != PARSER_SUCCESS) {
            return error;
        }
    } else {
        if (!parse_integer_value(parser, helper)) {
            if (helper->error != PARSER_SUCCESS) {
                return helper->error;
            }
            error = parse_action(parser, helper);
            if (error != PARSER_SUCCESS) {
                return error;
            }
        }
    }

    while (1) {
        instruction = 0;

        skip_space(parser);

        switch (parser->line[parser->column]) {
        case '\0':
            if (precedence == OPEN_BRACKET) {
                /* if a bracket is open, we look on the following lines for a
                 * closing bracket
                 */
                if (skip_space_and_new_lines(parser)) {
                    /* check if there is an operator on the next line */
                    if (strchr("&|)", parser->line[parser->column]) != NULL) {
                        continue;
                    }
                    /* implicit ; */
                    insert_instruction(helper, position, INSTRUCTION_NEXT);
                    error = parse_expression_recursively(parser, helper,
                            SEMICOLON);
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
            const char operator = parser->line[parser->column];
            uint32_t jump_offset;

            if (precedence > LOGICAL_AND) {
                return PARSER_SUCCESS;
            }
            parser->column++;
            if (parser->line[parser->column] != operator) {
                return PARSER_ERROR_UNEXPECTED;
            }
            parser->column++;
            jump_offset = helper->size;
            insert_instruction(helper, position, operator == '&' ?
                    INSTRUCTION_LOGICAL_AND : INSTRUCTION_LOGICAL_OR);
            (void) skip_space_and_new_lines(parser);
            error = parse_expression_recursively(parser, helper, LOGICAL_AND);
            if (error != PARSER_SUCCESS) {
                return error;
            }
            jump_offset = helper->size - jump_offset - 1;
            helper->instructions[position] |= jump_offset << 8;
            break;
        }

        /* separator operator */
        case ';':
            other_precedence = SEMICOLON;
            instruction = INSTRUCTION_NEXT;
            break;

        /* operators with PLUS precedence */
        case '+':
            other_precedence = PLUS;
            instruction = INSTRUCTION_ADD;
            break;
        case '-':
            other_precedence = PLUS;
            instruction = INSTRUCTION_SUBTRACT;
            break;

        /* operators with MULTIPLY precedence */
        case '*':
            other_precedence = MULTIPLY;
            instruction = INSTRUCTION_MULTIPLY;
            break;
        case '/':
            other_precedence = MULTIPLY;
            instruction = INSTRUCTION_DIVIDE;
            break;
        case '%':
            other_precedence = MULTIPLY;
            instruction = INSTRUCTION_MODULO;
            break;

        /* a closing bracket */
        case ')':
            if (precedence < OPEN_BRACKET) {
                return PARSER_ERROR_MISSING_OPENING_BRACKET;
            }
            /* let the higher function take care of the closing bracket */
            if (precedence > OPEN_BRACKET) {
                return PARSER_SUCCESS;
            }
            parser->column++;
            /* continue with a higher precedence */
            precedence++;
            break;

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
            insert_instruction(helper, position, instruction);
            (void) skip_space_and_new_lines(parser);
            error = parse_expression_recursively(parser, helper,
                    other_precedence);
            if (error != PARSER_SUCCESS) {
                return error;
            }
        }
    }
}

/* Parse an expression. */
parser_error_t parse_expression(Parser *parser, Expression *expression)
{
    parser_error_t error;
    struct helper helper;

    helper.error = PARSER_SUCCESS;
    helper.instructions = NULL;
    helper.size = 0;
    helper.capacity = 4;
    RESIZE(helper.instructions, helper.capacity);

    error = parse_expression_recursively(parser, &helper, ORIGIN);
    if (error == PARSER_SUCCESS) {
        RESIZE(helper.instructions, helper.size);
        expression->instructions = helper.instructions;
        expression->instruction_size = helper.size;
    } else {
        free(helper.instructions);
    }
    return error;
}
