#include <unistd.h>

#include "action.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"

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
        set_notification("No other window",
                g_cur_frame->x + g_cur_frame->w / 2,
                g_cur_frame->y + g_cur_frame->h / 2);
        return;
    }

    hide_window(window);
    g_cur_frame->window = next;
    show_window(next);
    set_focus_window(next);
}

static void prev_window(void)
{
    Window *window;
    Window *prev;

    window = g_cur_frame->window;
    prev = get_previous_hidden_window(window);
    if (prev == NULL) {
        set_notification("No other window",
                g_cur_frame->x + g_cur_frame->w / 2,
                g_cur_frame->y + g_cur_frame->h / 2);
        return;
    }

    hide_window(window);
    g_cur_frame->window = prev;
    show_window(prev);
    set_focus_window(prev);
}

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(int action)
{
    Window *window;

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

    case ACTION_SHOW_WINDOW_LIST:
        window = select_window_from_list();
        if (window != NULL) {
            if (!window->visible) {
                hide_window(g_cur_frame->window);
                g_cur_frame->window = window;
                show_window(window);
            }
            set_focus_window(window);
        }
        break;
    }
}
