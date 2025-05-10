#ifndef PARSE__STRUCT_H
#define PARSE__STRUCT_H

/**
 * This file is meant to be private to the parser.
 */

#include <setjmp.h>

#include "action.h"
#include "parse/data_type.h"
#include "parse/stream.h"
#include "utility/list.h"
#include "window.h"

/* The parser struct.  All memory allocated within this struct is kept for later
 * parsing runs/recycled.
 */
extern struct parser {
    /* the input stream object */
    InputStream *stream;
    /* the start index of the last item */
    size_t index;
    /* if the parser had any error */
    int error_count;
    /* the point to jump to */
    jmp_buf throw_jump;

    /* last read string */
    LIST(utf8_t, string);
    /* if this string has quotes */
    bool is_string_quoted;

    /* the current action parsing information */
    struct parse_action_information {
        /* Data for this action. */
        LIST(struct parse_generic_data, data);
        /* The offset within the string identifiers of the actions.
         * If this is -1, then the action was disregarded.
         */
        int offset;
    } actions[ACTION_MAX];
    /* the first/last (exclusive) action still valid within @offsets */
    action_type_t first_action, last_action;

    /* last read data */
    struct parse_generic_data data;

    /* the instance pattern of an association being parsed */
    LIST(utf8_t, instance_pattern);
    /* the class pattern of an association being parsed */
    LIST(utf8_t, class_pattern);

    /* the action items being parsed */
    LIST(struct action_list_item, action_items);
    /* data for the action list */
    LIST(struct parse_generic_data, action_data);

    /* the startup items being parsed */
    LIST(struct action_list_item, startup_items);
    /* data for the startup action list */
    LIST(struct parse_generic_data, startup_data);

    /* list of associations */
    LIST(struct window_association, associations);
} parser;

/* Emit a parse error. */
void emit_parse_error(const char *message);

/* Parse an action.
 *
 * Expects that a string has been read into `parser.`
 *
 * @return ERROR if there is no action, OK otherwise.
 */
int continue_parsing_action(void);

#endif
