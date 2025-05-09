#ifndef ACTION_H
#define ACTION_H

/**
 * Actions expose internal functionality to the user.
 *
 * The user can invoke any actions in any order at any time.
 */

#include "bits/actions.h"

/* action codes */
typedef enum {
#define X(identifier, string) \
    identifier,
    DEFINE_ALL_PARSE_ACTIONS
#undef X
    /* these get special treatment by the parser */
    ACTION_BUTTON_BINDING,
    ACTION_KEY_BINDING,
    /* not a real action */
    ACTION_MAX,
} action_type_t;

/* forward declaration... */
struct parse_generic_data;

/* A list of actions. */
struct action_list {
    /* all items within the list */
    struct action_list_item {
        /* the type of this actions */
        action_type_t type;
        /* the number of data points in `data` */
        unsigned data_count;
    } *items;
    /* the number of items in `items` */
    unsigned number_of_items;
    /* the data associated to the actions */
    struct parse_generic_data *data;
};

/* Get the action string of given action type. */
const char *get_action_string(action_type_t type);

/* Do all actions within @list. */
void run_action_list(const struct action_list *list);

/* Make a deep copy of @list and put it into itself. */
void duplicate_action_list(struct action_list *list);

/* Free very deep memory associated to the action list @list. */
void clear_action_list_but_keep_shallow(struct action_list *list);

/* Free ALL memory associated to the action list @list and set it to 0. */
void clear_action_list(struct action_list *list);

/* Log a list of actions to stderr. */
void log_action_list(const struct action_list *list);

/* Do the given action using given @data. */
void do_action(action_type_t type, const struct parse_generic_data *data);

#endif
