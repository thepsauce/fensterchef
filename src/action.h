#ifndef ACTION_H
#define ACTION_H

enum {
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
    /* ... */
    ACTION_REMOVE_FRAME,
    /* ... */
    ACTION_SPLIT_HORIZONTALLY,
    /* ... */
    ACTION_SPLIT_VERTICALLY,
    /* ... */
    ACTION_MOVE_UP,
    /* ... */
    ACTION_MOVE_LEFT,
    /* ... */
    ACTION_MOVE_RIGHT,
    /* ... */
    ACTION_MOVE_DOWN,
    /* show the interactive window list */
    ACTION_SHOW_WINDOW_LIST,
};

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(int action);

#endif
