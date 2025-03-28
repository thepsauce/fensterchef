#ifndef ACTION_H
#define ACTION_H

/**
 * Actions expose internal functionality to the user.
 *
 * The user can invoke any actions in any order at any time.
 */

#include "bits/window_typedef.h"

#include "data_type.h"

/* expands to all actions */
#define DECLARE_ALL_ACTIONS \
    /* invalid action value */ \
    X(ACTION_NULL, false, NULL, 0) \
\
    /* no action at all */ \
    X(ACTION_NONE, false, "none", DATA_TYPE_VOID) \
    /* reload the configuration file */ \
    X(ACTION_RELOAD_CONFIGURATION, false, "reload-configuration", DATA_TYPE_VOID) \
    /* assign a number to a frame */ \
    X(ACTION_ASSIGN, false, "assign", DATA_TYPE_INTEGER) \
    /* focus a frame with given number */ \
    X(ACTION_FOCUS_FRAME, true, "focus-frame", DATA_TYPE_INTEGER) \
    /* move the focus to the parent frame */ \
    X(ACTION_FOCUS_PARENT, true, "focus-parent", DATA_TYPE_INTEGER) \
    /* move the focus to the child frame */ \
    X(ACTION_FOCUS_CHILD, true, "focus-child", DATA_TYPE_INTEGER) \
    /* equalize the size of the child frames within a frame */ \
    X(ACTION_EQUALIZE_FRAME, false, "equalize-frame", DATA_TYPE_VOID) \
    /* closes the currently active window */ \
    X(ACTION_CLOSE_WINDOW, false, "close-window", DATA_TYPE_VOID) \
    /* hides the window with given number of the clicked window */ \
    X(ACTION_MINIMIZE_WINDOW, true, "minimize-window", DATA_TYPE_INTEGER) \
    /* show the window with given number or the clicked window */ \
    X(ACTION_SHOW_WINDOW, true, "show-window", DATA_TYPE_INTEGER) \
    /* focus the window with given number or the clicked window */ \
    X(ACTION_FOCUS_WINDOW, true, "focus-window", DATA_TYPE_INTEGER) \
    /* start moving a window with the mouse */ \
    X(ACTION_INITIATE_MOVE, false, "initiate-move", DATA_TYPE_VOID) \
    /* start resizing a window with the mouse */ \
    X(ACTION_INITIATE_RESIZE, false, "initiate-resize", DATA_TYPE_VOID) \
    /* go to the next window in the window list */ \
    X(ACTION_NEXT_WINDOW, true, "next-window", DATA_TYPE_INTEGER) \
    /* go to the previous window in the window list */ \
    X(ACTION_PREVIOUS_WINDOW, true, "previous-window", DATA_TYPE_INTEGER) \
    /* remove the current frame */ \
    X(ACTION_REMOVE_FRAME, false, "remove-frame", DATA_TYPE_VOID) \
    /* remove the current frame and replace it with a frame from the stash */ \
    X(ACTION_OTHER_FRAME, false, "other-frame", DATA_TYPE_VOID) \
    /* changes a non tiling window to a tiling window and vise versa */ \
    X(ACTION_TOGGLE_TILING, false, "toggle-tiling", DATA_TYPE_VOID) \
    /* toggles the fullscreen state of the currently focused window */ \
    X(ACTION_TOGGLE_FULLSCREEN, false, "toggle-fullscreen", DATA_TYPE_VOID) \
    /* change the focus from tiling to non tiling or vise versa */ \
    X(ACTION_TOGGLE_FOCUS, false, "toggle-focus", DATA_TYPE_VOID) \
    /* split the current frame horizontally */ \
    X(ACTION_SPLIT_HORIZONTALLY, false, "split-horizontally", DATA_TYPE_VOID) \
    /* split the current frame vertically */ \
    X(ACTION_SPLIT_VERTICALLY, false, "split-vertically", DATA_TYPE_VOID) \
    /* split the current frame left horizontally */ \
    X(ACTION_LEFT_SPLIT_HORIZONTALLY, false, "left-split-horizontally", DATA_TYPE_VOID) \
    /* split the current frame left vertically */ \
    X(ACTION_LEFT_SPLIT_VERTICALLY, false, "left-split-vertically", DATA_TYPE_VOID) \
    /* hint that the current frame should split horizontally */ \
    X(ACTION_HINT_SPLIT_HORIZONTALLY, false, "hint-split-horizontally", DATA_TYPE_VOID) \
    /* hint that the current frame should split vertically */ \
    X(ACTION_HINT_SPLIT_VERTICALLY, false, "hint-split-vertically", DATA_TYPE_VOID) \
    /* move the focus to the above frame */ \
    X(ACTION_FOCUS_UP, false, "focus-up", DATA_TYPE_VOID) \
    /* move the focus to the left frame */ \
    X(ACTION_FOCUS_LEFT, false, "focus-left", DATA_TYPE_VOID) \
    /* move the focus to the right frame */ \
    X(ACTION_FOCUS_RIGHT, false, "focus-right", DATA_TYPE_VOID) \
    /* move the focus to the frame below */ \
    X(ACTION_FOCUS_DOWN, false, "focus-down", DATA_TYPE_VOID) \
    /* exchange the current frame with the above one */ \
    X(ACTION_EXCHANGE_UP, false, "exchange-up", DATA_TYPE_VOID) \
    /* exchange the current frame with the left one */ \
    X(ACTION_EXCHANGE_LEFT, false, "exchange-left", DATA_TYPE_VOID) \
    /* exchange the current frame with the right one */ \
    X(ACTION_EXCHANGE_RIGHT, false, "exchange-right", DATA_TYPE_VOID) \
    /* exchange the current frame with the below one */ \
    X(ACTION_EXCHANGE_DOWN, false, "exchange-down", DATA_TYPE_VOID) \
    /* move the current frame up */ \
    X(ACTION_MOVE_UP, false, "move-up", DATA_TYPE_VOID) \
    /* move the current frame to the left */ \
    X(ACTION_MOVE_LEFT, false, "move-left", DATA_TYPE_VOID) \
    /* move the current frame to the right */ \
    X(ACTION_MOVE_RIGHT, false, "move-right", DATA_TYPE_VOID) \
    /* move the current frame down */ \
    X(ACTION_MOVE_DOWN, false, "move-down", DATA_TYPE_VOID) \
    /* show the interactive window list */ \
    X(ACTION_SHOW_WINDOW_LIST, false, "show-window-list", DATA_TYPE_VOID) \
    /* run a shell program */ \
    X(ACTION_RUN, false, "run", DATA_TYPE_STRING) \
    /* show a notification with a string message */ \
    X(ACTION_SHOW_MESSAGE, false, "show-message", DATA_TYPE_STRING) \
    /* show a notification with a message extracted from a shell program */ \
    X(ACTION_SHOW_MESSAGE_RUN, false, "show-message-run", DATA_TYPE_STRING) \
    /* resize the edges of the current window by given values */ \
    X(ACTION_RESIZE_BY, false, "resize-by", DATA_TYPE_QUAD) \
    /* resize the edges of the current window to given values */ \
    X(ACTION_RESIZE_TO, false, "resize-to", DATA_TYPE_QUAD) \
    /* center a window to given monitor (glob pattern) or the monitor the window is currently on */ \
    X(ACTION_CENTER_TO, true, "center-to", DATA_TYPE_STRING) \
    /* write all fensterchef information to a file */ \
    X(ACTION_DUMP_LAYOUT, false, "dump-layout", DATA_TYPE_STRING) \
    /* quit fensterchef */ \
    X(ACTION_QUIT, false, "quit", DATA_TYPE_VOID)

/* action codes */
typedef enum {
#define X(identifier, is_optional, string, data_type) \
    identifier,
    DECLARE_ALL_ACTIONS
#undef X
    /* not a real action */
    ACTION_MAX,

    /* the first valid action value */
    ACTION_FIRST_ACTION = ACTION_NULL + 1
} action_type_t;

/* all actions and their string representation and data type */
extern const struct action_information {
    /* name of the action */
    const char *name;
    /* if the argument of this action should be optional */
    bool is_argument_optional;
    /* data type of the action data */
    data_type_t data_type;
} action_information[ACTION_MAX];

/* Check if the given action's argument may be omitted. */
static inline bool has_action_optional_argument(action_type_t action)
{
    return action_information[action].is_argument_optional;
}

/* Get the data type the action expects as parameter. */
static inline data_type_t get_action_data_type(action_type_t action)
{
    return action_information[action].data_type;
}

/* Get a string version of an action. */
static inline const char *action_type_to_string(action_type_t action)
{
    return action_information[action].name;
}

/* Get an action from a string (case insensitive). */
action_type_t string_to_action_type(const char *string);

/* Get a string version of an action. */
const char *action_type_to_string(action_type_t action);

/* Do the given action on the given window.
 *
 * @return the status of the action. Usually false when the action had no
 *         visible result or could not be executed at all.
 */
bool do_action(action_type_t type, GenericData *data);

#endif
