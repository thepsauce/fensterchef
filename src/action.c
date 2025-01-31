#include <unistd.h>

#include "action.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "tiling.h"
#include "window_list.h"

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
                g_cur_frame->x + g_cur_frame->width / 2,
                g_cur_frame->y + g_cur_frame->height / 2);
        return;
    }

    set_window_state(window, WINDOW_STATE_SHOWN, 1);
    (void) set_focus_window(window);
}

/* Changes the focus from popup to tiling and vise versa. */
static void change_focus(void)
{
    Window *window;

    window = get_focus_window();
    if (window == NULL) {
        return;
    }

    if (window->state.current == WINDOW_STATE_POPUP) {
        set_focus_frame(g_cur_frame);
    } else {
        for (Window *w = g_first_window; w != NULL; w = w->next) {
            if (w != window && w->state.current == WINDOW_STATE_POPUP &&
                    set_focus_window(w) == 0) {
                break;
            }
        }
    }
}

/* Shows the user the window list and lets the user select a window to focus. */
static void show_window_list(void)
{
    Window *window;

    window = select_window_from_list();
    if (window == NULL) {
        return;
    }

    switch (window->state.current) {
    case WINDOW_STATE_HIDDEN:
        set_window_state(window, WINDOW_STATE_SHOWN, 1);
        break;
    case WINDOW_STATE_SHOWN:
        set_focus_frame(window->frame);
        break;
    case WINDOW_STATE_POPUP:
    case WINDOW_STATE_FULLSCREEN:
        set_window_above(window);
        set_focus_window(window);
        break;
    }
}

/* Do the given action, the action codes are `ACTION_*`. */
void do_action(action_t action)
{
    Window *window;
    Frame *frame;

    switch (action) {
    /* invalid action value */
    case ACTION_NULL:
        ERR("tried to do NULL action\n");
        break;

    /* open a terminal window */
    case ACTION_START_TERMINAL:
        start_terminal();
        break;

    /* go to the next window in the window list */
    case ACTION_NEXT_WINDOW:
        set_active_window(get_next_hidden_window(g_cur_frame->window));
        break;

    /* go to the previous window in the window list */
    case ACTION_PREV_WINDOW:
        set_active_window(
                get_previous_hidden_window(g_cur_frame->window));
        break;

    /* remove the current frame */
    /* TODO: make it so this hides a popup window as well? */
    case ACTION_REMOVE_FRAME:
        if (remove_frame(g_cur_frame) != 0) {
            set_notification(UTF8_TEXT("Can not remove the last frame"),
                    g_cur_frame->x + g_cur_frame->width / 2,
                    g_cur_frame->y + g_cur_frame->height / 2);
        }
        break;

    /* changes a popup window to a tiling window and vise versa */
    case ACTION_CHANGE_WINDOW_STATE:
        window = get_focus_window();
        if (window == NULL) {
            break;
        }
        set_window_state(window, window->state.current == WINDOW_STATE_SHOWN ?
                WINDOW_STATE_POPUP : WINDOW_STATE_SHOWN, 1);
        break;

    /* changes from focusing a popup window to focusing a tiling window */
    case ACTION_CHANGE_FOCUS:
        change_focus();
        break;

    /* toggles the fullscreen state of the currently focused window */
    case ACTION_TOGGLE_FULLSCREEN:
        window = get_focus_window();
        if (window != NULL) {
            set_window_state(window,
                    window->state.current == WINDOW_STATE_FULLSCREEN ?
                    window->state.previous : WINDOW_STATE_FULLSCREEN, 1);
        }
        break;

    /* split the current frame horizontally */
    case ACTION_SPLIT_HORIZONTALLY:
        split_frame(g_cur_frame, 0);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_VERTICALLY:
        split_frame(g_cur_frame, 1);
        break;

    /* move to the frame above the current one */
    case ACTION_MOVE_UP:
        frame = get_frame_at_position(g_cur_frame->x, g_cur_frame->y - 1);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame left of the current one */
    case ACTION_MOVE_LEFT:
        frame = get_frame_at_position(g_cur_frame->x - 1, g_cur_frame->y);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame right of the current one */
    case ACTION_MOVE_RIGHT:
        frame = get_frame_at_position(g_cur_frame->x + g_cur_frame->width,
                g_cur_frame->y);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame below the current one */
    case ACTION_MOVE_DOWN:
        frame = get_frame_at_position(g_cur_frame->x,
                g_cur_frame->y + g_cur_frame->height);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* show the interactive window list */
    case ACTION_SHOW_WINDOW_LIST:
        show_window_list();
        break;

    /* quits the window manager */
    case ACTION_QUIT_WM:
        quit_fensterchef(0);
        break;
    }

    LOG("performed action: %u\n", action);
}
