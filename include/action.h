#ifndef ACTION_H
#define ACTION_H

#include "bits/configuration_parser_data_type.h"
#include "bits/window_typedef.h"

/* expands to all actions */
#define DECLARE_ALL_ACTIONS \
    /* invalid action value */ \
    X(ACTION_NULL, false, NULL, 0) \
 \
    /* no action at all */ \
    X(ACTION_NONE, false, "NONE", PARSER_DATA_TYPE_VOID) \
    /* reload the configuration file */ \
    X(ACTION_RELOAD_CONFIGURATION, false, "RELOAD-CONFIGURATION", PARSER_DATA_TYPE_VOID) \
    /* assign a number to a frame */ \
    X(ACTION_ASSIGN, false, "ASSIGN", PARSER_DATA_TYPE_INTEGER) \
    /* focus a frame with given number */ \
    X(ACTION_FOCUS_FRAME, true, "FOCUS-FRAME", PARSER_DATA_TYPE_INTEGER) \
    /* move the focus to the parent frame */ \
    X(ACTION_FOCUS_PARENT, true, "FOCUS-PARENT", PARSER_DATA_TYPE_INTEGER) \
    /* move the focus to the child frame */ \
    X(ACTION_FOCUS_CHILD, true, "FOCUS-CHILD", PARSER_DATA_TYPE_INTEGER) \
    /* equalize the size of the child frames within a frame */ \
    X(ACTION_EQUALIZE_FRAME, false, "EQUALIZE-FRAME", PARSER_DATA_TYPE_VOID) \
    /* closes the currently active window */ \
    X(ACTION_CLOSE_WINDOW, false, "CLOSE-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* hides the currently active window */ \
    X(ACTION_MINIMIZE_WINDOW, false, "MINIMIZE-WINDOW", PARSER_DATA_TYPE_VOID) \
    /* focus the window with given number or the clicked window */ \
    X(ACTION_FOCUS_WINDOW, true, "FOCUS-WINDOW", PARSER_DATA_TYPE_INTEGER) \
    /* start moving a window with the mouse */ \
    X(ACTION_INITIATE_MOVE, false, "INITIATE-MOVE", PARSER_DATA_TYPE_VOID) \
    /* start resizing a window with the mouse */ \
    X(ACTION_INITIATE_RESIZE, false, "INITIATE-RESIZE", PARSER_DATA_TYPE_VOID) \
    /* go to the next window in the window list */ \
    X(ACTION_NEXT_WINDOW, true, "NEXT-WINDOW", PARSER_DATA_TYPE_INTEGER) \
    /* go to the previous window in the window list */ \
    X(ACTION_PREVIOUS_WINDOW, true, "PREVIOUS-WINDOW", PARSER_DATA_TYPE_INTEGER) \
    /* remove the current frame */ \
    X(ACTION_REMOVE_FRAME, false, "REMOVE-FRAME", PARSER_DATA_TYPE_VOID) \
    /* changes a non tiling window to a tiling window and vise versa */ \
    X(ACTION_TOGGLE_TILING, false, "TOGGLE-TILING", PARSER_DATA_TYPE_VOID) \
    /* toggles the fullscreen state of the currently focused window */ \
    X(ACTION_TOGGLE_FULLSCREEN, false, "TOGGLE-FULLSCREEN", PARSER_DATA_TYPE_VOID) \
    /* change the focus from tiling to non tiling or vise versa */ \
    X(ACTION_TOGGLE_FOCUS, false, "TOGGLE-FOCUS", PARSER_DATA_TYPE_VOID) \
    /* split the current frame horizontally */ \
    X(ACTION_SPLIT_HORIZONTALLY, false, "SPLIT-HORIZONTALLY", PARSER_DATA_TYPE_VOID) \
    /* split the current frame vertically */ \
    X(ACTION_SPLIT_VERTICALLY, false, "SPLIT-VERTICALLY", PARSER_DATA_TYPE_VOID) \
    /* hint that the current frame should split horizontally */ \
    X(ACTION_HINT_SPLIT_HORIZONTALLY, false, "HINT-SPLIT-HORIZONTALLY", PARSER_DATA_TYPE_VOID) \
    /* hint that the current frame should split vertically */ \
    X(ACTION_HINT_SPLIT_VERTICALLY, false, "HINT-SPLIT-VERTICALLY", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the above frame */ \
    X(ACTION_FOCUS_UP, false, "FOCUS-UP", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the left frame */ \
    X(ACTION_FOCUS_LEFT, false, "FOCUS-LEFT", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the right frame */ \
    X(ACTION_FOCUS_RIGHT, false, "FOCUS-RIGHT", PARSER_DATA_TYPE_VOID) \
    /* move the focus to the frame below */ \
    X(ACTION_FOCUS_DOWN, false, "FOCUS-DOWN", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the above one */ \
    X(ACTION_EXCHANGE_UP, false, "EXCHANGE-UP", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the left one */ \
    X(ACTION_EXCHANGE_LEFT, false, "EXCHANGE-LEFT", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the right one */ \
    X(ACTION_EXCHANGE_RIGHT, false, "EXCHANGE-RIGHT", PARSER_DATA_TYPE_VOID) \
    /* exchange the current frame with the below one */ \
    X(ACTION_EXCHANGE_DOWN, false, "EXCHANGE-DOWN", PARSER_DATA_TYPE_VOID) \
    /* show the interactive window list */ \
    X(ACTION_SHOW_WINDOW_LIST, false, "SHOW-WINDOW-LIST", PARSER_DATA_TYPE_VOID) \
    /* run a shell program */ \
    X(ACTION_RUN, false, "RUN", PARSER_DATA_TYPE_STRING) \
    /* show a notification with a string message */ \
    X(ACTION_SHOW_MESSAGE, false, "SHOW-MESSAGE", PARSER_DATA_TYPE_STRING) \
    /* show a notification with a message extracted from a shell program */ \
    X(ACTION_SHOW_MESSAGE_RUN, false, "SHOW-MESSAGE-RUN", PARSER_DATA_TYPE_STRING) \
    /* resize the edges of the current window */ \
    X(ACTION_RESIZE_BY, false, "RESIZE-BY", PARSER_DATA_TYPE_QUAD) \
    /* quit fensterchef */ \
    X(ACTION_QUIT, false, "QUIT", PARSER_DATA_TYPE_VOID)

/* action codes */
typedef enum {
#define X(code, is_optional, string, data_type) code,
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

/* Check if the given action's argument may be omitted. */
bool has_action_optional_argument(action_t action);

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

/* Do the given action on the given window.
 *
 * @return the status of the action. Usually false when the action had no
 *         visible result or could not be executed at all.
 */
bool do_action(const Action *action, Window *window);

#endif
