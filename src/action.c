#include <inttypes.h>
#include <unistd.h> // fork(), setsid(), _exit(), execl()
#include <string.h> // strcasecmp()
#include <sys/wait.h> // waitpid()

#include "action.h"
#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "move_frame.h"
#include "size_frame.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window_list.h"

/* all actions and their string representation and data type */
const struct action_information action_information[ACTION_MAX] = {
#define X(code, is_optional, string, data_type) [code] = \
    { string, is_optional, data_type },
    DECLARE_ALL_ACTIONS
#undef X
};

/* Get an action from a string. */
action_t string_to_action(const char *string)
{
    for (action_t i = ACTION_FIRST_ACTION; i < ACTION_MAX; i++) {
        if (strcasecmp(action_information[i].name, string) == 0) {
            return i;
        }
    }
    return ACTION_NULL;
}

/* Create a deep copy of given action array. */
Action *duplicate_actions(Action *actions, uint32_t number_of_actions)
{
    Action *duplicate;

    duplicate = xmemdup(actions, sizeof(*actions) * number_of_actions);
    for (uint32_t i = 0; i < number_of_actions; i++) {
        duplicate_data_value(get_action_data_type(duplicate[i].code),
                &duplicate[i].data);
    }
    return duplicate;
}

/* Frees all given actions and the action array itself. */
void free_actions(Action *actions, uint32_t number_of_actions)
{
    for (uint32_t i = 0; i < number_of_actions; i++) {
        clear_data_value(get_action_data_type(actions[i].code),
                &actions[i].data);
    }
    free(actions);
}

/* Run given shell program. */
static void run_shell(const char *shell)
{
    int child_process_id;

    /* using `fork()` twice and `_exit()` will give the child process to the
     * init system so we do not need to worry about cleaning up dead child
     * processes; we want to run the shell in a new session
     * TODO: explain why `setsid()` is needed
     */

    /* create a child process */
    child_process_id = fork();
    if (child_process_id == 0) {
        /* this code is executed in the child */

        /* create a grandchild process */
        if (fork() == 0) {
            /* make a new session */
            if (setsid() == -1) {
                /* TODO: when does this happen? */
                exit(EXIT_FAILURE);
            }
            /* this code is executed in the grandchild process */
            (void) execl("/bin/sh", "sh", "-c", shell, (char*) NULL);
            /* this point is only reached if `execl()` failed */
            exit(EXIT_FAILURE);
        } else {
            /* exit the child process */
            _exit(0);
        }
    } else {
        /* wait until the child process exits */
        (void) waitpid(child_process_id, NULL, 0);
    }
}

/* Run a shell and get the output. */
static char *run_shell_and_get_output(const char *shell)
{
    FILE *process;
    char *line;
    size_t length, capacity;

    process = popen(shell, "r");
    if (process == NULL) {
        return NULL;
    }

    capacity = 128;
    line = xmalloc(capacity);
    length = 0;
    /* read all output from the process, stopping at the end (EOF) or a new line
     */
    for (int c; (c = fgetc(process)) != EOF && c != '\n'; ) {
        if (length + 1 == capacity) {
            capacity *= 2;
            RESIZE(line, capacity);
        }
        line[length++] = c;
    }
    line[length] = '\0';
    pclose(process);
    return line;
}

/* Resize the current window or current frame if it does not exist. */
static bool resize_frame_or_window_by(Window *window, int32_t left, int32_t top,
        int32_t right, int32_t bottom)
{
    Frame *frame;

    if (window == NULL) {
        frame = Frame_focus;
        if (frame == NULL) {
            return false;
        }
    } else {
        frame = get_frame_of_window(window);
    }

    if (frame != NULL) {
        bump_frame_edge(frame, FRAME_EDGE_LEFT, left);
        bump_frame_edge(frame, FRAME_EDGE_TOP, top);
        bump_frame_edge(frame, FRAME_EDGE_RIGHT, right);
        bump_frame_edge(frame, FRAME_EDGE_BOTTOM, bottom);
    } else {
        right += left;
        bottom += top;
        /* check for underflows */
        if ((int32_t) window->width < -right) {
            right = -window->width;
        }
        if ((int32_t) window->height < -bottom) {
            bottom = -window->height;
        }
        set_window_size(window,
                window->x - left,
                window->y - top,
                window->width + right,
                window->height + bottom);
    }
    return true;
}

/* Get a tiling window that is not currently shown and put it into the focus
 * frame.
 *
 * @return if there is another window.
 */
bool set_showable_tiling_window(uint32_t count, bool previous)
{
    Window *start, *next, *valid_window = NULL;

    if (Frame_focus->window == NULL) {
        start = NULL;
        next = Window_first;
    } else {
        start = Frame_focus->window;
        next = start->next;
    }

    /* go through all windows in a cyclic manner */
    for (;; next = next->next) {
        /* wrap around */
        if (start != NULL && next == NULL) {
            next = Window_first;
        }

        /* check if we went around */
        if (next == start) {
            break;
        }

        if (!next->state.is_visible && next->state.mode == WINDOW_MODE_TILING) {
            valid_window = next;
            /* if the previous window is requested, we need to go further to
             * find the window before the current window
             */
            if (!previous) {
                count--;
                if (count == 0) {
                    break;
                }
            }
        }
    }

    if (valid_window == NULL) {
        set_notification((utf8_t*) "No other window",
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        return false;
    }

    /* clear the old frame and stash it */
    (void) stash_frame(Frame_focus);
    /* put the window into the focused frame, size and show it */
    Frame_focus->window = valid_window;
    reload_frame(Frame_focus);
    valid_window->state.is_visible = true;
    /* focus the shown window */
    set_focus_window(valid_window);
    return true;
}

/* Change the focus from tiling to non tiling and vise versa. */
bool toggle_focus(void)
{
    Window *window;

    /* Four cases must be handled:
     * 1. No window is focused
     * 1.1. There is a floating window
     * 1.2. There is no floating window
     * 2. A tiling window is focused
     * 3. A floating window is focused
     */

    if (Window_focus == NULL ||
            Window_focus->state.mode == WINDOW_MODE_TILING) {
        /* check for case 1.1 */
        for (window = Window_top; window != NULL; window = window->below) {
            if (window->state.mode == WINDOW_MODE_TILING) {
                break;
            }
            if (is_window_focusable(window) && window->state.is_visible) {
                /* cover case 1.1 */
                set_focus_window(window);
                return true;
            }
        }

        /* this covers case 1.2 and 2 */
        if (Frame_focus->window != NULL) {
            set_focus_window(Frame_focus->window);
        }
    } else if (Frame_focus->window != Window_focus) {
        /* cover case 3 */
        set_focus_window(Frame_focus->window);
        return true;
    }
    return false;
}

/* Move the focus from @from to @to and exchange if requested. */
static void move_to_frame(Frame *from, Frame *to, Monitor *monitor,
        bool do_exchange)
{
    if (do_exchange) {
        exchange_frames(from, to);
    /* check if a window is covering the monitor */
    } else if (monitor != NULL) {
        Window *const window = get_window_covering_monitor(monitor);
        if (window != NULL) {
            set_focus_window(window);
            Frame_focus = to;
        } else {
            set_focus_frame(to);
        }
    /* simply "move" to the next frame by focusing it */
    } else {
        set_focus_frame(to);
    }
}

/* Move the focus to the frame above @relative. */
static bool move_to_above_frame(Frame *relative, bool do_exchange)
{
    Frame *frame;
    Monitor *monitor = NULL;

    /* if a group of frames is given, get a frame inside if the split direction
     * is aligned with the movement
     */
    if (!do_exchange && relative->left != NULL &&
            relative->split_direction == FRAME_SPLIT_VERTICALLY) {
        frame = relative->left;
    } else {
        frame = get_above_frame(relative);
        if (frame == NULL) {
            /* move across monitors */
            monitor = get_monitor_containing_frame(relative);
            monitor = get_above_monitor(monitor);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    frame = get_bottom_leaf_frame(frame, relative->x + relative->width / 2);
    move_to_frame(relative, frame, monitor, do_exchange);
    return true;
}

/* Move the focus to the frame left of @relative. */
static bool move_to_left_frame(Frame *relative, bool do_exchange)
{
    Frame *frame;
    Monitor *monitor = NULL;

    /* if a group of frames is given, get a frame inside if the split direction
     * is aligned with the movement
     */
    if (!do_exchange && relative->left != NULL &&
            relative->split_direction == FRAME_SPLIT_HORIZONTALLY) {
        frame = relative->left;
    } else {
        frame = get_left_frame(relative);
        if (frame == NULL) {
            /* move across monitors */
            monitor = get_monitor_containing_frame(relative);
            monitor = get_left_monitor(monitor);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    frame = get_most_right_leaf_frame(frame,
            relative->y + relative->height / 2);
    move_to_frame(relative, frame, monitor, do_exchange);
    return true;
}

/* Move the focus to the frame right of @relative. */
static bool move_to_right_frame(Frame *relative, bool do_exchange)
{
    Frame *frame;
    Monitor *monitor = NULL;

    /* if a group of frames is given, get a frame inside if the split direction
     * is aligned with the movement
     */
    if (!do_exchange && relative->left != NULL &&
            relative->split_direction == FRAME_SPLIT_HORIZONTALLY) {
        frame = relative->right;
    } else {
        frame = get_right_frame(relative);
        if (frame == NULL) {
            /* move across monitors */
            monitor = get_monitor_containing_frame(relative);
            monitor = get_right_monitor(monitor);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    frame = get_most_left_leaf_frame(frame, relative->y + relative->height / 2);
    move_to_frame(relative, frame, monitor, do_exchange);
    return true;
}

/* Move the focus to the frame below @relative. */
static bool move_to_below_frame(Frame *relative, bool do_exchange)
{
    Frame *frame;
    Monitor *monitor = NULL;

    /* if a group of frames is given, get a frame inside if the split direction
     * is aligned with the movement
     */
    if (!do_exchange && relative->left != NULL &&
            Frame_focus->split_direction == FRAME_SPLIT_VERTICALLY) {
        frame = Frame_focus->right;
    } else {
        frame = get_below_frame(relative);
        if (frame == NULL) {
            /* move across monitors */
            monitor = get_monitor_containing_frame(relative);
            monitor = get_below_monitor(monitor);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    frame = get_top_leaf_frame(frame, relative->y + relative->height / 2);
    move_to_frame(relative, frame, monitor, do_exchange);
    return true;
}

/* Do the given action. */
bool do_action(const Action *action, Window *window)
{
    char *shell;
    int32_t count;
    Frame *frame;
    bool is_previous = true;

    switch (action->code) {
    /* invalid action value */
    case ACTION_NULL:
        LOG_ERROR("tried to do NULL action");
        return false;

    /* do nothing */
    case ACTION_NONE:
        return false;

    /* reload the configuration file */
    case ACTION_RELOAD_CONFIGURATION:
        /* this needs to be delayed because if this is called by a binding and
         * the configuration is immediately reloaded, the pointer to the binding
         * becomes invalid and a crash occurs
         */
        is_reload_requested = true;
        break;

    /* assign a number to a frame */
    case ACTION_ASSIGN:
        /* remove the number from the old frame if there is any */
        frame = get_frame_by_number((uint32_t) action->data.integer);
        /* also try to find it in the stash */
        if (frame == NULL) {
            frame = Frame_last_stashed;
            for (; frame != NULL; frame = frame->previous_stashed) {
                if (frame->number == (uint32_t) action->data.integer) {
                    break;
                }
            }
        }
        if (frame != NULL) {
            frame->number = 0;
        }

        Frame_focus->number = action->data.integer;
        if (Frame_focus->number == 0) {
            set_notification((utf8_t*) "Number removed",
                    Frame_focus->x + Frame_focus->width / 2,
                    Frame_focus->y + Frame_focus->height / 2);
        } else {
            char number[MAXIMUM_DIGITS(Frame_focus->number) + 1];

            snprintf(number, sizeof(number), "%" PRIu32, Frame_focus->number);
            set_notification((utf8_t*) number,
                    Frame_focus->x + Frame_focus->width / 2,
                    Frame_focus->y + Frame_focus->height / 2);
        }
        break;

    /* focus a frame with given number */
    case ACTION_FOCUS_FRAME:
        frame = get_frame_by_number((uint32_t) action->data.integer);
        /* check if the frame is already shown */
        if (frame != NULL) {
            set_focus_frame(frame);
            break;
        }

        frame = Frame_last_stashed;
        /* also try to find it in the stash */
        for (; frame != NULL; frame = frame->previous_stashed) {
            if (frame->number == (uint32_t) action->data.integer) {
                break;
            }
        }

        if (frame == NULL) {
            break;
        }

        /* make the frame no longer stashed */
        unlink_frame_from_stash(frame);
        /* clear the old frame and stash it */
        (void) stash_frame(Frame_focus);
        /* put the new frame into the focused frame */
        replace_frame(Frame_focus, frame);
        /* destroy this now empty frame */
        destroy_frame(frame);
        /* focus a window that might have appeared */
        set_focus_window(Frame_focus->window);
        break;

    /* move the focus to the parent frame */
    case ACTION_FOCUS_PARENT:
        count = action->data.integer;
        if (count == 0) {
            count = 1;
        }
        if (count < 0) {
            count = INT32_MAX;
        }
        /* move to the count'th parent */
        for (frame = Frame_focus; frame->parent != NULL && count > 0; count--) {
            if (frame == frame->parent->left) {
                frame->parent->moved_from_left = true;
            } else {
                frame->parent->moved_from_left = false;
            }
            frame = frame->parent;
        }

        if (frame == Frame_focus) {
            return false;
        }

        Frame_focus = frame;
        break;

    /* move the focus to the child frame */
    case ACTION_FOCUS_CHILD:
        count = action->data.integer;
        if (count == 0) {
            count = 1;
        }
        if (count < 0) {
            count = INT32_MAX;
        }
        /* move to the count'th child */
        for (frame = Frame_focus; frame->left != NULL && count > 0; count--) {
            if (frame->moved_from_left) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        }

        if (frame == Frame_focus) {
            return false;
        }

        Frame_focus = frame;
        break;

    /* equalize the size of the child frames within a frame */
    case ACTION_EQUALIZE_FRAME:
        equalize_frame(Frame_focus, FRAME_SPLIT_HORIZONTALLY);
        equalize_frame(Frame_focus, FRAME_SPLIT_VERTICALLY);
        break;

    /* closes the currently active window */
    case ACTION_CLOSE_WINDOW:
        if (window == NULL) {
            return false;
        }
        close_window(window);
        break;

    /* hide the currently active window */
    case ACTION_MINIMIZE_WINDOW:
        if (window == NULL) {
            return false;
        }
        hide_window(window);
        break;

    /* show a window */
    case ACTION_SHOW_WINDOW:
        if (action->data.integer != 0) {
            for (window = Window_first; window != NULL; window = window->next) {
                if (window->number == (uint32_t) action->data.integer) {
                    break;
                }
            }
        }

        if (window == NULL || window == Window_focus) {
            return false;
        }

        show_window(window);
        update_window_layer(window);
        break;

    /* focus a window */
    case ACTION_FOCUS_WINDOW:
        if (action->data.integer != 0) {
            for (window = Window_first; window != NULL; window = window->next) {
                if (window->number == (uint32_t) action->data.integer) {
                    break;
                }
            }
        }

        if (window == NULL || window == Window_focus) {
            return false;
        }

        show_window(window);
        update_window_layer(window);
        set_focus_window_with_frame(window);
        break;

    /* start moving a window with the mouse */
    case ACTION_INITIATE_MOVE:
        if (window == NULL) {
            return false;
        }
        initiate_window_move_resize(window, _NET_WM_MOVERESIZE_MOVE, -1, -1);
        break;

    /* start resizing a window with the mouse */
    case ACTION_INITIATE_RESIZE:
        if (window == NULL) {
            return false;
        }
        initiate_window_move_resize(window, _NET_WM_MOVERESIZE_AUTO, -1, -1);
        break;

    /* go to the next window in the window list */
    case ACTION_NEXT_WINDOW:
        is_previous = false;
        /* fall through */
    /* go to the previous window in the window list */
    case ACTION_PREVIOUS_WINDOW:
        count = action->data.integer;
        if (count == 0) {
            count = 1;
        }
        if (count < 0) {
            count *= -1;
            is_previous = !is_previous;
        }
        return set_showable_tiling_window(count, is_previous);

    /* remove the current frame */
    case ACTION_REMOVE_FRAME:
        frame = Frame_focus;
        (void) stash_frame(frame);
        /* do not remove a root frame */
        if (frame->parent != NULL) {
            remove_frame(frame);
            destroy_frame(frame);
        }

        /* if nothing is focused/no longer focused, focus the window within the
         * current frame
         */
        if (Window_focus == NULL) {
            set_focus_window(Frame_focus->window);
        }
        break;

    /* remove the current frame and replace it with a frame from the stash */
    case ACTION_OTHER_FRAME: {
        Frame *const pop = pop_stashed_frame();
        if (pop == NULL) {
            return false;
        }
        stash_frame(Frame_focus);
        replace_frame(Frame_focus, pop);
        destroy_frame(pop);
        /* focus any window that might have appeared */
        set_focus_window(Frame_focus->window);
        break;
    }

    /* changes a non tiling window to a tiling window and vise versa */
    case ACTION_TOGGLE_TILING:
        if (window == NULL) {
            return false;
        }
        set_window_mode(window,
                window->state.mode == WINDOW_MODE_TILING ?
                WINDOW_MODE_FLOATING : WINDOW_MODE_TILING);
        break;

    /* toggles the fullscreen state of the currently focused window */
    case ACTION_TOGGLE_FULLSCREEN:
        if (window == NULL) {
            return false;
        }
        set_window_mode(window,
                window->state.mode == WINDOW_MODE_FULLSCREEN ?
                (window->state.previous_mode == WINDOW_MODE_FULLSCREEN ?
                    WINDOW_MODE_FLOATING : window->state.previous_mode) :
                WINDOW_MODE_FULLSCREEN);
        break;

    /* change the focus from tiling to non tiling or vise versa */
    case ACTION_TOGGLE_FOCUS:
        return toggle_focus();

    /* split the current frame horizontally */
    case ACTION_SPLIT_HORIZONTALLY:
        split_frame(Frame_focus, NULL, false, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_VERTICALLY:
        split_frame(Frame_focus, NULL, false, FRAME_SPLIT_VERTICALLY);
        break;

    /* split the current frame horizontally */
    case ACTION_LEFT_SPLIT_HORIZONTALLY:
        split_frame(Frame_focus, NULL, true, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame vertically */
    case ACTION_LEFT_SPLIT_VERTICALLY:
        split_frame(Frame_focus, NULL, true, FRAME_SPLIT_VERTICALLY);
        break;

    /* split the current frame horizontally */
    case ACTION_HINT_SPLIT_HORIZONTALLY:
        Frame_focus->split_direction = FRAME_SPLIT_HORIZONTALLY;
        /* reload the children if any */
        resize_frame(Frame_focus, Frame_focus->x, Frame_focus->y,
                Frame_focus->width, Frame_focus->height);
        break;

    /* split the current frame vertically */
    case ACTION_HINT_SPLIT_VERTICALLY:
        Frame_focus->split_direction = FRAME_SPLIT_VERTICALLY;
        /* reload the children if any */
        resize_frame(Frame_focus, Frame_focus->x, Frame_focus->y,
                Frame_focus->width, Frame_focus->height);
        break;

    /* move the focus to the frame above */
    case ACTION_FOCUS_UP:
        return move_to_above_frame(Frame_focus, false);

    /* move the focus to the left frame */
    case ACTION_FOCUS_LEFT:
        return move_to_left_frame(Frame_focus, false);

    /* move the focus to the right frame */
    case ACTION_FOCUS_RIGHT:
        return move_to_right_frame(Frame_focus, false);

    /* move the focus to the frame below */
    case ACTION_FOCUS_DOWN:
        return move_to_below_frame(Frame_focus, false);

    /* exchange the current frame with the above one */
    case ACTION_EXCHANGE_UP:
        return move_to_above_frame(Frame_focus, true);

    /* exchange the current frame with the left one */
    case ACTION_EXCHANGE_LEFT:
        return move_to_left_frame(Frame_focus, true);

    /* exchange the current frame with the right one */
    case ACTION_EXCHANGE_RIGHT:
        return move_to_right_frame(Frame_focus, true);

    /* exchange the current frame with the below one */
    case ACTION_EXCHANGE_DOWN:
        return move_to_below_frame(Frame_focus, true);

    /* move the current frame up */
    case ACTION_MOVE_UP:
        return move_frame_up(Frame_focus);

    /* move the current frame to the left */
    case ACTION_MOVE_LEFT:
        return move_frame_left(Frame_focus);

    /* move the current frame to the right */
    case ACTION_MOVE_RIGHT:
        return move_frame_right(Frame_focus);

    /* move the current frame down */
    case ACTION_MOVE_DOWN:
        return move_frame_down(Frame_focus);

    /* toggle visibility of the interactive window list */
    case ACTION_SHOW_WINDOW_LIST:
        if (show_window_list() == ERROR) {
            unmap_client(&window_list.client);
            return false;
        }
        break;

    /* run a shell program */
    case ACTION_RUN:
        run_shell((char*) action->data.string);
        break;

    /* show the user a message */
    case ACTION_SHOW_MESSAGE:
        set_notification((utf8_t*) action->data.string,
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        break;

    /* show a message by getting output from a shell script */
    case ACTION_SHOW_MESSAGE_RUN:
        shell = run_shell_and_get_output((char*) action->data.string);
        if (shell == NULL) {
            return false;
        }
        set_notification((utf8_t*) shell,
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        free(shell);
        break;

    /* resize the edges of the current window */
    case ACTION_RESIZE_BY:
        return resize_frame_or_window_by(window,
                action->data.quad[0],
                action->data.quad[1],
                action->data.quad[2],
                action->data.quad[3]);

    /* resize the edges of the current window */
    case ACTION_RESIZE_TO: {
        Monitor *monitor;

        monitor = get_monitor_from_rectangle_or_primary(window->x,
                window->y, window->width, window->height);
        return resize_frame_or_window_by(window,
                window->x - (monitor->x + action->data.quad[0]),
                window->y - (monitor->y + action->data.quad[1]),
                monitor->x + action->data.quad[2] - window->x - window->width,
                monitor->y + action->data.quad[3] - window->y - window->height);
    }

    /* center a window to given monitor (glob pattern) */
    case ACTION_CENTER_TO: {
        Monitor *monitor;

        if (window == NULL) {
            return false;
        }

        if (action->data.string == NULL) {
            monitor = get_monitor_from_rectangle(window->x,
                    window->y, window->width, window->height);
        } else {
            monitor = get_monitor_by_pattern((char*) action->data.string);
        }

        if (monitor == NULL) {
            return false;
        }

        set_window_size(window,
                monitor->x + (monitor->width - window->width) / 2,
                monitor->y + (monitor->height - window->height) / 2,
                window->width, window->height);
        break;
    }

    /* quit fensterchef */
    case ACTION_QUIT:
        Fensterchef_is_running = false;
        break;

    /* not a real action */
    case ACTION_MAX:
        break;
    }
    return true;
}
