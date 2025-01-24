#include <unistd.h>

#include "action.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"

/* Start a terminal.
 */
static void start_terminal(void)
{
    if (fork() == 0) {
        execl("/usr/bin/xterm", "/usr/bin/xterm", NULL);
    }
}

/* Show the given window and focus it.
 *
 * @window can be NULL to emit a message to the user.
 */
static void set_active_window(Window *window)
{
    if (window == NULL) {
        set_notification(UTF8_TEXT("No other window"),
                g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
                g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);
        return;
    }

    set_window_state(window, WINDOW_STATE_SHOWN, 1);
    set_focus_window(window);
}

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(action_t action)
{
    Window  *window;
    Frame   frame;

    switch (action) {
    case ACTION_NULL:
        ERR("tried to do NULL action\n");
        break;

    case ACTION_START_TERMINAL:
        start_terminal();
        break;

    case ACTION_NEXT_WINDOW:
        set_active_window(get_next_hidden_window(g_frames[g_cur_frame].window));
        break;

    case ACTION_PREV_WINDOW:
        set_active_window(
                get_previous_hidden_window(g_frames[g_cur_frame].window));
        break;

    case ACTION_REMOVE_FRAME:
        if (remove_leaf_frame(g_cur_frame) != 0) {
            set_notification(UTF8_TEXT("Can not remove the last frame"),
                g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
                g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);
        }
        break;

    case ACTION_CHANGE_WINDOW_STATE:
        window = get_focus_window();
        if (window == NULL) {
            break;
        }
        set_window_state(window, window->state == WINDOW_STATE_SHOWN ?
                WINDOW_STATE_POPUP : WINDOW_STATE_SHOWN, 1);
        break;

    case ACTION_CHANGE_FOCUS:
        window = get_focus_window();
        if (window == NULL) {
            break;
        }
        if (window->state == WINDOW_STATE_POPUP) {
            if (g_frames[g_cur_frame].window != NULL) {
                set_focus_window(g_frames[g_cur_frame].window);
            } else {
                for (frame = 0; frame < g_frame_capacity; frame++) {
                    if (g_frames[frame].window == NULL ||
                            g_frames[frame].window == WINDOW_SENTINEL) {
                        continue;
                    }
                    set_focus_frame(frame);
                }
            }
        } else {
            for (Window *w = g_first_window; w != NULL; w = w->next) {
                if (w == window || w->state != WINDOW_STATE_POPUP) {
                    continue;
                }
                set_focus_window(w);
                break;
            }
        }
        break;

    case ACTION_TOGGLE_FULLSCREEN:
        window = get_focus_window();
        if (window != NULL) {
            /* TODO: maybe use saved old state? */
            set_window_state(window, window->state == WINDOW_STATE_FULLSCREEN ?
                    WINDOW_STATE_POPUP : WINDOW_STATE_FULLSCREEN, 1);
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
            switch (window->state) {
            case WINDOW_STATE_HIDDEN:
                set_window_state(window, WINDOW_STATE_SHOWN, 1);
                set_focus_window(window);
                break;
            case WINDOW_STATE_SHOWN:
                set_focus_frame(get_frame_of_window(window));
                break;
            case WINDOW_STATE_POPUP:
            case WINDOW_STATE_FULLSCREEN:
                g_values[0] = XCB_STACK_MODE_ABOVE;
                xcb_configure_window(g_dpy, window->xcb_window,
                    XCB_CONFIG_WINDOW_STACK_MODE, g_values);
                set_focus_window(window);
                break;
            }
        }
        break;

    case ACTION_QUIT_WM:
        quit_fensterchef(0);
        break;
    }
}
