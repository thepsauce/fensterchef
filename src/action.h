#ifndef ACTION_H
#define ACTION_H

enum {
    /* open a terminal window */
    ACTION_START_TERMINAL,
    /* go to the next window in the window list */
    ACTION_NEXT_WINDOW,
    /* go to the previous window in the window list */
    ACTION_PREV_WINDOW,
};

/* do the given action, codes are above */
void do_action(int action);

#endif
