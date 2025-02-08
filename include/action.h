#ifndef ACTION_H
#define ACTION_H

/* action codes */
typedef enum {
    /* invalid action value */
    ACTION_NULL,

    /* the first valid action value */
    ACTION_FIRST_ACTION,

    /* no action at all */
    ACTION_NONE = ACTION_FIRST_ACTION,
    /* reload the configuration file */
    ACTION_RELOAD_CONFIGURATION,
    /* open a terminal window */
    ACTION_START_TERMINAL,
    /* go to the next window in the window list */
    ACTION_NEXT_WINDOW,
    /* go to the previous window in the window list */
    ACTION_PREV_WINDOW,
    /* remove the current frame */
    ACTION_REMOVE_FRAME,
    /* changes a popup window to a tiling window and vise versa */
    ACTION_CHANGE_WINDOW_STATE,
    /* changes the window that was in focus before the current one */
    ACTION_TRAVERSE_FOCUS,
    /* toggles the fullscreen state of the currently focused window */
    ACTION_TOGGLE_FULLSCREEN,
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
    /* show the interactive window list */
    ACTION_SHOW_WINDOW_LIST,
    /* quits the window manager */
    ACTION_QUIT_WM,

    /* not a real action */
    ACTION_MAX
} action_t;

/* Get an action from a string (case insensitive). */
action_t convert_string_to_action(const char *string);

/* Get a string version of an action. */
const char *convert_action_to_string(action_t action);

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(action_t action);

#endif
