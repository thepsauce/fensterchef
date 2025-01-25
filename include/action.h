#ifndef ACTION_H
#define ACTION_H

/* action codes */
typedef enum {
    /* invalid action value */
    ACTION_NULL,
    /* open a terminal window */
    ACTION_START_TERMINAL,
    /* go to the next window in the window list */
    ACTION_NEXT_WINDOW,
    /* go to the previous window in the window list */
    ACTION_PREV_WINDOW,
    /* changes a popup window to a tiling window and vise versa */
    ACTION_CHANGE_WINDOW_STATE,
    /* changes from focusing a popup window to focusing a tiling window */
    ACTION_CHANGE_FOCUS,
    /* toggles the fullscreen state of the currently focused window */
    ACTION_TOGGLE_FULLSCREEN,
    /* remove the current frame */
    ACTION_REMOVE_FRAME,
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
} action_t;

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(action_t action);

#endif
