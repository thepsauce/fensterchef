#ifndef ACTION_H
#define ACTION_H

enum {
    /* open a terminal window */
    ACTION_START_TERMINAL,
    /* go to the next window in the window list */
    ACTION_NEXT_WINDOW,
    /* go to the previous window in the window list */
    ACTION_PREV_WINDOW,
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
};

/* do the given action, codes are above */
void do_action(int action);

#endif
