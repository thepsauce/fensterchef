#include <inttypes.h>
#include <unistd.h>
#include <string.h> // strcmp()
#include <sys/wait.h> // wait()

#include "action.h"
#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window_list.h"

/* all actions and their string representation and data type */
static const struct {
    /* name of the action */
    const char *name;
    /* if the argument of this action should be optional */
    bool is_argument_optional;
    /* data type of the action parameter */
    parser_data_type_t data_type;
} action_information[ACTION_MAX] = {
#define X(code, is_optional, string, data_type) [code] = \
    { string, is_optional, data_type },
    DECLARE_ALL_ACTIONS
#undef X
};

/* Check if the given action's argument may be omitted. */
inline bool has_action_optional_argument(action_t action)
{
    return action_information[action].is_argument_optional;
}

/* Get the data type the action expects as parameter. */
inline parser_data_type_t get_action_data_type(action_t action)
{
    return action_information[action].data_type;
}

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

/* Get a string version of an action. */
inline const char *action_to_string(action_t action)
{
    return action_information[action].name;
}

/* Create a deep copy of given action array. */
Action *duplicate_actions(Action *actions, uint32_t number_of_actions)
{
    Action *duplicate;

    duplicate = xmemdup(actions, sizeof(*actions) * number_of_actions);
    for (uint32_t i = 0; i < number_of_actions; i++) {
        duplicate_data_value(get_action_data_type(duplicate[i].code),
                &duplicate[i].parameter);
    }
    return duplicate;
}

/* Frees all given actions and the action array itself. */
void free_actions(Action *actions, uint32_t number_of_actions)
{
    for (uint32_t i = 0; i < number_of_actions; i++) {
        clear_data_value(get_action_data_type(actions[i].code),
                &actions[i].parameter);
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
        frame = focus_frame;
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
bool set_showable_tiling_window(bool previous)
{
    Window *start, *next, *valid_window = NULL;

    if (focus_frame->window == NULL) {
        start = NULL;
        next = first_window;
    } else {
        start = focus_frame->window;
        next = start->next;
    }

    /* go through all windows in a cyclic manner */
    for (;; next = next->next) {
        /* wrap around */
        if (start != NULL && next == NULL) {
            next = first_window;
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
                break;
            }
        }
    }

    if (valid_window == NULL) {
        set_notification((utf8_t*) "No other window",
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        return false;
    }

    /* clear the old frame and stash it */
    (void) stash_frame(focus_frame);
    /* put the window into the focused frame, size and show it */
    focus_frame->window = valid_window;
    reload_frame(focus_frame);
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

    if (focus_window == NULL ||
            focus_window->state.mode == WINDOW_MODE_TILING) {
        /* check for case 1.1 */
        for (window = top_window; window != NULL; window = window->below) {
            if (window->state.mode == WINDOW_MODE_TILING) {
                break;
            }
            if (window->state.is_visible) {
                /* cover case 1.1 */
                set_focus_window(window);
                return true;
            }
        }

        /* this covers case 1.2 and 2 */
        if (focus_frame->window != NULL) {
            set_focus_window(focus_frame->window);
        }
    } else if (focus_frame->window != focus_window) {
        /* cover case 3 */
        set_focus_window(focus_frame->window);
        return true;
    }
    return false;
}

/* Move the focus from @from to @to and exchange if requested. */
static void move_to_frame(Frame *from, Frame *to, Monitor *monitor,
        bool do_exchange)
{
    if (do_exchange) {
        /* if moving into a void, either remove it or replace it */
        if (is_frame_void(to)) {
            if (to->parent != NULL &&
                    (configuration.tiling.auto_remove ||
                     configuration.tiling.auto_remove_void)) {
                remove_void(to);
                /* focus stays at `from` */
            } else {
                replace_frame(to, from);
                set_focus_frame(to);
            }
        /* swap the two frames `from` and `to` */
        } else {
            Frame saved_frame;

            saved_frame = *from;
            replace_frame(from, to);
            replace_frame(to, &saved_frame);
            set_focus_frame(to);
        }
    /* check if a window is covering the monitor */
    } else if (monitor != NULL) {
        Window *const window = get_window_covering_monitor(monitor);
        if (window != NULL) {
            set_focus_window(window);
            focus_frame = to;
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
            monitor = get_monitor_from_rectangle(
                    relative->x, relative->y - 1, 1, 1);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    const int x = relative->x + relative->width / 2;
    /* move into the most bottom frame */
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_HORIZONTALLY) {
            if (frame->left->x + (int32_t) frame->left->width >= x) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->right;
        }
    }

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
            monitor = get_monitor_from_rectangle(
                    relative->x - 1, relative->y, 1, 1);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    const int y = relative->y + relative->height / 2;
    /* move into the most right frame */
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_VERTICALLY) {
            if (frame->left->y + (int32_t) frame->left->height >= y) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->right;
        }
    }

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
            monitor = get_monitor_from_rectangle(
                    relative->x + relative->width, relative->y, 1, 1);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    const int y = relative->y + relative->height / 2;
    /* move into the most left frame */
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_VERTICALLY) {
            if (frame->left->y + (int32_t) frame->left->height >= y) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->left;
        }
    }

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
            focus_frame->split_direction == FRAME_SPLIT_VERTICALLY) {
        frame = focus_frame->right;
    } else {
        frame = get_below_frame(relative);
        if (frame == NULL) {
            monitor = get_monitor_from_rectangle(
                    relative->x, relative->y + relative->height, 1, 1);
            if (monitor != NULL) {
                frame = monitor->frame;
            }
        }
    }

    if (frame == NULL) {
        return false;
    }

    const int x = relative->x + relative->width / 2;
    /* move into the most top frame */
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_HORIZONTALLY) {
            if (frame->left->x + (int32_t) frame->left->width >= x) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->left;
        }
    }

    move_to_frame(relative, frame, monitor, do_exchange);
    return true;
}

/* Do the given action. */
bool do_action(const Action *action, Window *window)
{
    char *shell;
    uint32_t count;
    Frame *frame;

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
        frame = get_frame_by_number((uint32_t) action->parameter.integer);
        /* also try to find it in the stash */
        if (frame == NULL) {
            frame = last_stashed_frame;
            for (; frame != NULL; frame = frame->previous_stashed) {
                if (frame->number == (uint32_t) action->parameter.integer) {
                    break;
                }
            }
        }
        if (frame != NULL) {
            frame->number = 0;
        }

        focus_frame->number = action->parameter.integer;
        if (focus_frame->number == 0) {
            set_notification((utf8_t*) "Number removed",
                    focus_frame->x + focus_frame->width / 2,
                    focus_frame->y + focus_frame->height / 2);
        } else {
            char number[APPROXIMATE_DIGITS(focus_frame->number) + 1];

            snprintf(number, sizeof(number), "%" PRIu32, focus_frame->number);
            set_notification((utf8_t*) number,
                    focus_frame->x + focus_frame->width / 2,
                    focus_frame->y + focus_frame->height / 2);
        }
        break;

    /* focus a frame with given number */
    case ACTION_FOCUS_FRAME:
        frame = get_frame_by_number((uint32_t) action->parameter.integer);
        /* check if the frame is already shown */
        if (frame != NULL) {
            set_focus_frame(frame);
            break;
        }

        frame = last_stashed_frame;
        /* also try to find it in the stash */
        for (; frame != NULL; frame = frame->previous_stashed) {
            if (frame->number == (uint32_t) action->parameter.integer) {
                break;
            }
        }

        if (frame == NULL) {
            break;
        }

        /* make the frame no longer stashed */
        unlink_frame_from_stash(frame);
        /* clear the old frame and stash it */
        (void) stash_frame(focus_frame);
        /* put the new frame into the focused frame */
        replace_frame(focus_frame, frame);
        /* destroy this now empty frame */
        destroy_frame(frame);
        /* focus a window that might have appeared */
        set_focus_window(focus_frame->window);
        break;

    /* move the focus to the parent frame */
    case ACTION_FOCUS_PARENT:
        count = action->parameter.integer;
        if (count == 0) {
            count = 1;
        }
        /* move to the count'th parent */
        for (frame = focus_frame; frame->parent != NULL && count > 0; count--) {
            if (frame == frame->parent->left) {
                frame->parent->moved_from_left = true;
            } else {
                frame->parent->moved_from_left = false;
            }
            frame = frame->parent;
        }

        if (frame == focus_frame) {
            return false;
        }

        set_focus_frame(frame);
        break;

    /* move the focus to the child frame */
    case ACTION_FOCUS_CHILD:
        count = action->parameter.integer;
        if (count == 0) {
            count = 1;
        }
        /* move to the count'th child */
        for (frame = focus_frame; frame->left != NULL && count > 0; count--) {
            if (frame->moved_from_left) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        }

        if (frame == focus_frame) {
            return false;
        }

        set_focus_frame(frame);
        break;

    /* equalize the size of the child frames within a frame */
    case ACTION_EQUALIZE_FRAME:
        equalize_frame(focus_frame, FRAME_SPLIT_HORIZONTALLY);
        equalize_frame(focus_frame, FRAME_SPLIT_VERTICALLY);
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

    /* focus a window */
    case ACTION_FOCUS_WINDOW:
        if (action->parameter.integer == 0) {
            if (window == focus_window) {
                return false;
            }
            set_focus_window_with_frame(window);
            if (window != NULL) {
                update_window_layer(window);
            }
        } else {
            for (window = first_window; window != NULL; window = window->next) {
                if (window->number == (uint32_t) action->parameter.integer) {
                    break;
                }
            }
            if (window == NULL) {
                break;
            }
            show_window(window);
            set_focus_window_with_frame(window);
            update_window_layer(window);
        }
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
        count = action->parameter.integer;
        if (count == 0) {
            count = 1;
        }
        for (; count > 0; count--) {
            if (!set_showable_tiling_window(false)) {
                return false;
            }
        }
        break;

    /* go to the previous window in the window list */
    case ACTION_PREVIOUS_WINDOW:
        count = action->parameter.integer;
        if (count == 0) {
            count = 1;
        }
        for (; count > 0; count--) {
            if (!set_showable_tiling_window(true)) {
                return false;
            }
        }
        break;

    /* remove the current frame */
    case ACTION_REMOVE_FRAME:
        (void) stash_frame(focus_frame);
        (void) remove_void(focus_frame);
        if (focus_window == NULL) {
            set_focus_window(focus_frame->window);
        }
        break;

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
        split_frame(focus_frame, NULL, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_VERTICALLY:
        split_frame(focus_frame, NULL, FRAME_SPLIT_VERTICALLY);
        break;

    /* split the current frame horizontally */
    case ACTION_HINT_SPLIT_HORIZONTALLY:
        focus_frame->split_direction = FRAME_SPLIT_HORIZONTALLY;
        break;

    /* split the current frame vertically */
    case ACTION_HINT_SPLIT_VERTICALLY:
        focus_frame->split_direction = FRAME_SPLIT_VERTICALLY;
        break;

    /* move the focus to the frame above */
    case ACTION_FOCUS_UP:
        return move_to_above_frame(focus_frame, false);

    /* move the focus to the left frame */
    case ACTION_FOCUS_LEFT:
        return move_to_left_frame(focus_frame, false);

    /* move the focus to the right frame */
    case ACTION_FOCUS_RIGHT:
        return move_to_right_frame(focus_frame, false);

    /* move the focus to the frame below */
    case ACTION_FOCUS_DOWN:
        return move_to_below_frame(focus_frame, false);

    /* exchange the current frame with the above one */
    case ACTION_EXCHANGE_UP:
        return move_to_above_frame(focus_frame, true);

    /* exchange the current frame with the left one */
    case ACTION_EXCHANGE_LEFT:
        return move_to_left_frame(focus_frame, true);

    /* exchange the current frame with the right one */
    case ACTION_EXCHANGE_RIGHT:
        return move_to_right_frame(focus_frame, true);

    /* exchange the current frame with the below one */
    case ACTION_EXCHANGE_DOWN:
        return move_to_below_frame(focus_frame, true);

    /* toggle visibility of the interactive window list */
    case ACTION_SHOW_WINDOW_LIST:
        if (show_window_list() == ERROR) {
            unmap_client(&window_list.client);
            return false;
        }
        break;

    /* quit fensterchef */
    case ACTION_QUIT:
        is_fensterchef_running = false;
        break;

    /* run a shell program */
    case ACTION_RUN:
        run_shell((char*) action->parameter.string);
        break;

    /* show the user a message */
    case ACTION_SHOW_MESSAGE:
        set_notification((utf8_t*) action->parameter.string,
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        break;

    /* show a message by getting output from a shell script */
    case ACTION_SHOW_MESSAGE_RUN:
        shell = run_shell_and_get_output((char*) action->parameter.string);
        if (shell == NULL) {
            return false;
        }
        set_notification((utf8_t*) shell,
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        free(shell);
        break;

    /* resize the edges of the current window */
    case ACTION_RESIZE_BY:
        return resize_frame_or_window_by(window,
                action->parameter.quad[0],
                action->parameter.quad[1],
                action->parameter.quad[2],
                action->parameter.quad[3]);

    /* not a real action */
    case ACTION_MAX:
        break;
    }
    return true;
}
