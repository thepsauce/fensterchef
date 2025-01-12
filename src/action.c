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
    if (window == NULL) {
        return;
    }
    next = window;
    do {
        if (next->next == NULL) {
            next = g_first_window;
        } else {
            next = next->next;
        }
        if (window == next) {
            /* TODO: notify user that there is no other window */
            return;
        }
    } while (next->visible);

    hide_window(window);
    g_cur_frame->window = next;
    show_window(next);
}

static void prev_window(void)
{
    Window *window;
    Window *prev, *prev_prev;

    window = g_cur_frame->window;
    if (window == NULL) {
        return;
    }
    prev = window;
    do {
        for (prev_prev = g_first_window; prev_prev->next != prev; ) {
            if (prev_prev->next == NULL) {
                break;
            }
            prev_prev = prev_prev->next;
        }
        prev = prev_prev;
        if (window == prev) {
            /* TODO: notify user that there is no other window */
            return;
        }
    } while (prev->visible);

    hide_window(window);
    g_cur_frame->window = prev;
    show_window(prev);
}

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
