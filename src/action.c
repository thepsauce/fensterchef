#include <unistd.h>

#include "action.h"
#include "frame.h"

static void start_terminal(void)
{
    if (fork() == 0) {
        execl("/usr/bin/xterm", "/usr/bin/xterm", NULL);
    }
}

static void next_window(void)
{
    Window *window;
    Window *next;

    window = g_cur_frame->window;
    next = get_next_hidden_window(window);
    if (next == NULL) {
        return;
    }

    hide_window(window);
    g_cur_frame->window = next;
    show_window(next);
}

static void prev_window(void)
{
    Window *window;
    Window *prev;

    window = g_cur_frame->window;
    prev = get_prev_hidden_window(window);
    if (prev == NULL) {
        return;
    }

    hide_window(window);
    g_cur_frame->window = prev;
    show_window(prev);
}

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(int action)
{
    switch (action) {
    case ACTION_START_TERMINAL:
        start_terminal();
        break;
    case ACTION_NEXT_WINDOW:
        next_window();
        break;
    case ACTION_PREV_WINDOW:
        prev_window();
        break;
    }
}
