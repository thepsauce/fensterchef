#ifndef ACTION_H
#define ACTION_H

#include "bits/configuration_parser_data_type.h"
#include "bits/window_typedef.h"

/* action codes
 *
 * NOTE: After editing an action code, also edit the action_information[] array
 * in action.c and implement the action in do_action().
 */
typedef enum {
    /* invalid action value */
    ACTION_NULL,

    /* the first valid action value */
    ACTION_FIRST_ACTION,

    /* no action at all */
    ACTION_NONE = ACTION_FIRST_ACTION,
    /* reload the configuration file */
    ACTION_RELOAD_CONFIGURATION,
    /* move the focus to the parent frame */
    ACTION_PARENT_FRAME,
    /* move the focus to the child frame */
    ACTION_CHILD_FRAME,
    /* move the focus to the root frame */
    ACTION_ROOT_FRAME,
    /* closes the currently active window */
    ACTION_CLOSE_WINDOW,
    /* hides the currently active window */
    ACTION_MINIMIZE_WINDOW,
    /* focus a window */
    ACTION_FOCUS_WINDOW,
    /* start moving a window with the mouse */
    ACTION_INITIATE_MOVE,
    /* start resizing a window with the mouse */
    ACTION_INITIATE_RESIZE,
    /* go to the next window in the window list */
    ACTION_NEXT_WINDOW,
    /* go to the previous window in the window list */
    ACTION_PREVIOUS_WINDOW,
    /* remove the current frame */
    ACTION_REMOVE_FRAME,
    /* changes a non tiling window to a tiling window and vise versa */
    ACTION_TOGGLE_TILING,
    /* toggles the fullscreen state of the currently focused window */
    ACTION_TOGGLE_FULLSCREEN,
    /* change the focus from tiling to non tiling or vise versa */
    ACTION_TOGGLE_FOCUS,
    /* split the current frame horizontally */
    ACTION_SPLIT_HORIZONTALLY,
    /* split the current frame vertically */
    ACTION_SPLIT_VERTICALLY,
    /* move to the frame above the current one */
    ACTION_MOVE_UP,
    /* move to the frame left of the current one */
    ACTION_MOVE_LEFT,
    /* move to the frame right of the current one */
    ACTION_MOVE_RIGHT,
    /* move to the frame below the current one */
    ACTION_MOVE_DOWN,
    /* exchange the current frame with the above one */
    ACTION_EXCHANGE_UP,
    /* exchange the current frame with the left one */
    ACTION_EXCHANGE_LEFT,
    /* exchange the current frame with the right one */
    ACTION_EXCHANGE_RIGHT,
    /* exchange the current frame with the below one */
    ACTION_EXCHANGE_DOWN,
    /* show the interactive window list */
    ACTION_SHOW_WINDOW_LIST,
    /* run a shell program */
    ACTION_RUN,
    /* show a notification with a string message */
    ACTION_SHOW_MESSAGE,
    /* show a notification with a message extracted from a shell program */
    ACTION_SHOW_MESSAGE_RUN,
    /* resize the edges of the current window */
    ACTION_RESIZE_BY,
    /* quits fensterchef */
    ACTION_QUIT,

    /* not a real action */
    ACTION_MAX,
} action_t;

typedef struct action {
    /* the code of the action */
    action_t code;
    /* the parameter of the action */
    union parser_data_value parameter;
} Action;

/* Get the data type the action expects as parameter. */
parser_data_type_t get_action_data_type(action_t action);

/* Get an action from a string (case insensitive). */
action_t string_to_action(const char *string);

/* Get a string version of an action. */
const char *action_to_string(action_t action);

/* Create a deep copy of given action array. */
Action *duplicate_actions(Action *actions, uint32_t number_of_actions);

/* Free all given actions and the action array itself. */
void free_actions(Action *actions, uint32_t number_of_actions);

/* Do the given action on the given window. */
void do_action(const Action *action, Window *window);

#endif
