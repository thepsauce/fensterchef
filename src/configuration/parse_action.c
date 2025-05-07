#include <string.h>

#include "configuration/action.h"
#include "configuration/parse.h"
#include "configuration/parse_struct.h"
#include "configuration/stream.h"
#include "configuration/utility.h"
#include "utility/utility.h"

/* the corresponding string identifier for all actions */
const char *action_strings[ACTION_MAX] = {
#define X(identifier, string) \
    [identifier] = string,
    DEFINE_ALL_PARSE_ACTIONS
#undef X
};

/* Find a section in the action strings that match the word loaded into
 * `parser`.
 *
 * @return ERROR if no action matches.
 */
static int resolve_action_word(void)
{
    action_type_t count = 0;

    for (action_type_t i = 0; i < ACTION_MAX; i++) {
        const char *action, *space;
        unsigned skip_length;

        action = action_strings[i];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            skip_length = strlen(action);
            space = action + skip_length;
        } else {
            skip_length = space - action + 1;
        }

        if (parser.string_length == (size_t) (space - action) &&
                memcmp(action, parser.string,
                    parser.string_length) == 0) {
            if (count == 0) {
                parser.first_action = i;
            }

            count++;

            parser.last_action = i + 1;

            parser.actions[i].offset = skip_length;
            /* prepare the data for filling */
            parser.actions[i].data_length = 0;
        } else if (count > 0) {
            /* optimization: no more will match because of the alphabetical
             * sorting
             */
            break;
        }
    }

    return count == 0 ? ERROR : OK;
}

/* Read the next word and find the actions where the word matches.
 *
 * @return ERROR if no action matches.
 */
static int read_and_resolve_next_action_word(void)
{
    action_type_t count = 0;

    assert_read_string();

    /* go through all actions that matched previously */
    for (action_type_t i = parser.first_action, end = parser.last_action;
            i < end;
            i++) {
        const char *action, *space;
        unsigned skip_length;

        if (parser.actions[i].offset == -1) {
            continue;
        }

        action = &action_strings[i][parser.actions[i].offset];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            skip_length = strlen(action);
            space = action + skip_length;
        } else {
            skip_length = space - action + 1;
        }

        /* check if next is a string parameter */
        if (action[0] == 'S') {
            /* append the string data point */
            parser.data.flags = PARSE_DATA_FLAGS_IS_POINTER;
            LIST_COPY(parser.string, 0, parser.string_length + 1,
                    parser.data.u.string);
            LIST_APPEND_VALUE(parser.actions[i].data, parser.data);
        } else {
            /* the word needs to be unquoted */
            if (parser.is_string_quoted) {
                parser.actions[i].offset = -1;
                continue;
            }

            /* check if an integer is expected and try to resolve it */
            if (action[0] == 'I') {
                if (resolve_integer() == OK) {
                    /* append the integer data point */
                    LIST_APPEND_VALUE(parser.actions[i].data, parser.data);
                } else {
                    parser.actions[i].offset = -1;
                    continue;
                }
            } else {
                /* try to match the next word */
                if (parser.string_length != (size_t) (space - action) ||
                        memcmp(action, parser.string,
                            parser.string_length) != 0) {
                    parser.actions[i].offset = -1;
                    continue;
                }
            }
        }

        /* got a valid action */

        if (count == 0) {
            parser.first_action = i;
        }
        count++;

        parser.last_action = i + 1;

        parser.actions[i].offset += skip_length;
    }

    return count == 0 ? ERROR : OK;
}

/* Print all possible actions to stderr. */
static void print_action_possibilities(void)
{
    fprintf(stderr, "possible words are: ");
    for (action_type_t i = parser.first_action; i < parser.last_action; i++) {
        const char *action, *space;
        int length;

        action = &action_strings[i][parser.actions[i].offset];

        /* get the end of the next action word of the action string */
        space = strchr(action, ' ');
        if (space == NULL) {
            length = strlen(action);
            space = action + length;
        } else {
            length = space - action;
        }

        if (i != parser.first_action) {
            fprintf(stderr, ", ");
        }
        if (action[0] == 'I') {
            fprintf(stderr, "INTEGER");
        } else if (action[0] == 'S') {
            fprintf(stderr, "STRING");
        } else {
            fprintf(stderr, "%.*s",
                    length, action);
        }
    }
    fprintf(stderr, "\n");
}

/* Parse the next action word or check for an action separator.
 *
 * @return ERROR when there are no following actions, otherwise OK.
 */
static int parse_next_action_part(size_t item_index)
{
    int error = OK;
    int character;

    character = peek_stream_character();
    if (character == EOF || character == ',' || character == '\n') {
        if (action_strings[parser.first_action]
                [parser.actions[parser.first_action].offset] != '\0') {
            parser.index = input_stream.index;
            emit_parse_error("incomplete action");
            print_action_possibilities();
        } else {
            parser.action_items[item_index].type = parser.first_action;
            parser.action_items[item_index].data_count =
                parser.actions[parser.first_action].data_length;
            LIST_APPEND(parser.action_data,
                parser.actions[parser.first_action].data,
                parser.actions[parser.first_action].data_length);
        }

        if (character == ',') {
            /* skip ',' */
            (void) get_stream_character();
            skip_space();
            assert_read_string();
        } else {
            error = ERROR;
        }
    } else {
        if (read_and_resolve_next_action_word() == ERROR) {
            emit_parse_error("invalid action word");
            print_action_possibilities();
            skip_statement();
            error = ERROR;
        } else {
            error = parse_next_action_part(item_index);
        }
    }

    return error;
}

/* Parse an action.
 *
 * Expects that a string has been read into `parser.`
 *
 * @return ERROR if there is no action, OK otherwise.
 */
int continue_parsing_action(void)
{
    size_t item_index;

    parser.action_items_length = 0;
    parser.action_data_length = 0;

    do {
        item_index = parser.action_items_length;
        LIST_APPEND(parser.action_items, NULL, 1);

        if (resolve_action_word() != OK) {
            /* the action might be a binding or association */
            return ERROR;
        }
    } while (parse_next_action_part(item_index) == OK);

    return OK;
}
