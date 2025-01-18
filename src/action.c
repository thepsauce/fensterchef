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

    window = g_frames[g_cur_frame].window;
    next = get_next_hidden_window(window);
    if (next == NULL) {
        set_notification(UTF8_TEXT("No other window"),
                g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
                g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);
        return;
    }

    hide_window(window);
    g_frames[g_cur_frame].window = next;
    show_window(next);
    set_focus_window(next);
}

static void prev_window(void)
{
    Window *window;
    Window *prev;

    window = g_frames[g_cur_frame].window;
    prev = get_previous_hidden_window(window);
    if (prev == NULL) {
        set_notification(UTF8_TEXT("No other window"),
                g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
                g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);
        return;
    }

    hide_window(window);
    g_frames[g_cur_frame].window = prev;
    show_window(prev);
    set_focus_window(prev);
}

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(int action)
{
    Window  *window;
    Frame   frame;

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

    case ACTION_REMOVE_FRAME:
        if (remove_leaf_frame(g_cur_frame) != 0) {
            set_notification(UTF8_TEXT("Can not remove the last frame"),
                g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
                g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);
        }
        break;

    case ACTION_SPLIT_HORIZONTALLY:
        split_frame(g_cur_frame, 0);
        break;

    case ACTION_SPLIT_VERTICALLY:
        split_frame(g_cur_frame, 1);
        break;

    case ACTION_MOVE_UP:
        frame = get_frame_at_position(g_frames[g_cur_frame].x,
                g_frames[g_cur_frame].y - 1);
        if (frame != (Frame) -1) {
            set_focus_frame(frame);
        }
        break;

    case ACTION_MOVE_LEFT:
        frame = get_frame_at_position(g_frames[g_cur_frame].x - 1,
                g_frames[g_cur_frame].y);
        if (frame != (Frame) -1) {
            set_focus_frame(frame);
        }
        break;

    case ACTION_MOVE_RIGHT:
        frame = get_frame_at_position(
                    g_frames[g_cur_frame].x + g_frames[g_cur_frame].width,
                    g_frames[g_cur_frame].y);
        if (frame != (Frame) -1) {
            set_focus_frame(frame);
        }
        break;

    case ACTION_MOVE_DOWN:
        frame = get_frame_at_position(g_frames[g_cur_frame].x,
                    g_frames[g_cur_frame].y + g_frames[g_cur_frame].height);
        if (frame != (Frame) -1) {
            set_focus_frame(frame);
        }
        break;

    case ACTION_SHOW_WINDOW_LIST:
        window = select_window_from_list();
        if (window != NULL) {
            if (!window->visible) {
                if (g_frames[g_cur_frame].window != NULL) {
                    hide_window(g_frames[g_cur_frame].window);
                }
                g_frames[g_cur_frame].window = window;
                show_window(window);
            }
            set_focus_window(window);
        }
        break;
    }
}
