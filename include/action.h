#ifndef ACTION_H
#define ACTION_H

#include "bits/configuration_parser_data_type.h"
#include "bits/window_typedef.h"

/* expands to all actions */
#define DECLARE_ALL_ACTIONS \
    /* invalid action value */ \
    X(ACTION_NULL, NULL, 0) \
 \
    /* no action at all */ \
    X(ACTION_NONE, "NONE", PARSER_DATA_TYPE_VOID) \
    /* reload the configuration file */ \
    X(ACTION_RELOAD_CONFIGURATION, "RELOAD-CONFIGURATION", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the parent frame */ \
    X(ACTION_PARENT_FRAME, "PARENT-FRAME", PARSER_DATA_TYPE_INTEGER) \
    /* move the focus to the child frame */ \
    X(ACTION_CHILD_FRAME, "CHILD-FRAME", PARSER_DATA_TYPE_INTEGER) \
    /* equalize the size of the child frames within a frame */ \
    X(ACTION_EQUALIZE_FRAME, "EQUALIZE-FRAME", PARSER_DATA_TYPE_VOID) \
    /* closes the currently active window */ \
    X(ACTION_CLOSE_WINDOW, "CLOSE-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* hides the currently active window */ \
    X(ACTION_MINIMIZE_WINDOW, "MINIMIZE-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* focus a window */ \
    X(ACTION_FOCUS_WINDOW, "FOCUS-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* start moving a window with the mouse */ \
    X(ACTION_INITIATE_MOVE, "INITIATE-MOVE", PARSER_DATA_TYPE_VOID) \
    /* start resizing a window with the mouse */ \
    X(ACTION_INITIATE_RESIZE, "INITIATE-RESIZE", PARSER_DATA_TYPE_VOID) \
    /* go to the next window in the window list */ \
    X(ACTION_NEXT_WINDOW, "NEXT-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* go to the previous window in the window list */ \
    X(ACTION_PREVIOUS_WINDOW, "PREVIOUS-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* remove the current frame */ \
    X(ACTION_REMOVE_FRAME, "REMOVE-FRAME", PARSER_DATA_TYPE_VOID) \
    /* changes a non tiling window to a tiling window and vise versa */ \
    X(ACTION_TOGGLE_TILING, "TOGGLE-TILING", PARSER_DATA_TYPE_VOID) \
    /* toggles the fullscreen state of the currently focused window */ \
    X(ACTION_TOGGLE_FULLSCREEN, "TOGGLE-FULLSCREEN", PARSER_DATA_TYPE_VOID) \
    /* change the focus from tiling to non tiling or vise versa */ \
    X(ACTION_TOGGLE_FOCUS, "TOGGLE-FOCUS", PARSER_DATA_TYPE_VOID) \
    /* split the current frame horizontally */ \
    X(ACTION_SPLIT_HORIZONTALLY, "SPLIT-HORIZONTALLY", PARSER_DATA_TYPE_VOID) \
    /* split the current frame vertically */ \
    X(ACTION_SPLIT_VERTICALLY, "SPLIT-VERTICALLY", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the above frame */ \
    X(ACTION_FOCUS_UP, "FOCUS-UP", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the left frame */ \
    X(ACTION_FOCUS_LEFT, "FOCUS-LEFT", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the right frame */ \
    X(ACTION_FOCUS_RIGHT, "FOCUS-RIGHT", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the frame below */ \
    X(ACTION_FOCUS_DOWN, "FOCUS-DOWN", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the above one */ \
    X(ACTION_EXCHANGE_UP, "EXCHANGE-UP", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the left one */ \
    X(ACTION_EXCHANGE_LEFT, "EXCHANGE-LEFT", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the right one */ \
    X(ACTION_EXCHANGE_RIGHT, "EXCHANGE-RIGHT", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the below one */ \
    X(ACTION_EXCHANGE_DOWN, "EXCHANGE-DOWN", PARSER_DATA_TYPE_VOID) \
    /* show the interactive window list */ \
    X(ACTION_SHOW_WINDOW_LIST, "SHOW-WINDOW-LIST", PARSER_DATA_TYPE_VOID) \
    /* run a shell program */ \
    X(ACTION_RUN, "RUN", PARSER_DATA_TYPE_STRING) \
    /* show a notification with a string message */ \
    X(ACTION_SHOW_MESSAGE, "SHOW-MESSAGE", PARSER_DATA_TYPE_STRING) \
    /* show a notification with a message extracted from a shell program */ \
    X(ACTION_SHOW_MESSAGE_RUN, "SHOW-MESSAGE-RUN", PARSER_DATA_TYPE_STRING) \
    /* resize the edges of the current window */ \
    X(ACTION_RESIZE_BY, "RESIZE-BY", PARSER_DATA_TYPE_QUAD) \
    /* quit fensterchef */ \
    X(ACTION_QUIT, "QUIT", PARSER_DATA_TYPE_VOID)

/* action codes */
typedef enum {
#define X(code, string, data_type) code,
    DECLARE_ALL_ACTIONS
#undef X
    /* not a real action */
    ACTION_MAX,

    /* the first valid action value */
    ACTION_FIRST_ACTION = ACTION_NULL + 1
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
