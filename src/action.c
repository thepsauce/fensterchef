#include <unistd.h>
#include <string.h> // strcmp()
#include <sys/wait.h> // wait()

#include "action.h"
#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window_list.h"

/* all actions and their string representation */
static const struct {
    /* name of the action */
    const char *name;
    /* data type of the action parameter */
    parser_data_type_t data_type;
} action_information[ACTION_MAX] = {
    [ACTION_NULL] = { NULL, 0 },

    [ACTION_NONE] = { "NONE", PARSER_DATA_TYPE_VOID },
    [ACTION_RELOAD_CONFIGURATION] = { "RELOAD-CONFIGURATION", PARSER_DATA_TYPE_VOID },
    [ACTION_PARENT] = { "PARENT", PARSER_DATA_TYPE_VOID },
    [ACTION_CHILD] = { "CHILD", PARSER_DATA_TYPE_VOID },
    [ACTION_CLOSE_WINDOW] = { "CLOSE-WINDOW", PARSER_DATA_TYPE_VOID },
    [ACTION_MINIMIZE_WINDOW] = { "MINIMIZE-WINDOW", PARSER_DATA_TYPE_VOID },
    [ACTION_FOCUS_WINDOW] = { "FOCUS-WINDOW", PARSER_DATA_TYPE_VOID },
    [ACTION_INITIATE_MOVE] = { "INITIATE-MOVE", PARSER_DATA_TYPE_VOID },
    [ACTION_INITIATE_RESIZE] = { "INITIATE-RESIZE", PARSER_DATA_TYPE_VOID },
    [ACTION_NEXT_WINDOW] = { "NEXT-WINDOW", PARSER_DATA_TYPE_VOID },
    [ACTION_PREVIOUS_WINDOW] = { "PREVIOUS-WINDOW", PARSER_DATA_TYPE_VOID },
    [ACTION_REMOVE_FRAME] = { "REMOVE-FRAME", PARSER_DATA_TYPE_VOID },
    [ACTION_TOGGLE_TILING] = { "TOGGLE-TILING", PARSER_DATA_TYPE_VOID },
    [ACTION_TOGGLE_FULLSCREEN] = { "TOGGLE-FULLSCREEN", PARSER_DATA_TYPE_VOID },
    [ACTION_TOGGLE_FOCUS] = { "TOGGLE-FOCUS", PARSER_DATA_TYPE_VOID },
    [ACTION_SPLIT_HORIZONTALLY] = { "SPLIT-HORIZONTALLY", PARSER_DATA_TYPE_VOID },
    [ACTION_SPLIT_VERTICALLY] = { "SPLIT-VERTICALLY", PARSER_DATA_TYPE_VOID },
    [ACTION_MOVE_UP] = { "MOVE-UP", PARSER_DATA_TYPE_VOID },
    [ACTION_MOVE_LEFT] = { "MOVE-LEFT", PARSER_DATA_TYPE_VOID },
    [ACTION_MOVE_RIGHT] = { "MOVE-RIGHT", PARSER_DATA_TYPE_VOID },
    [ACTION_MOVE_DOWN] = { "MOVE-DOWN", PARSER_DATA_TYPE_VOID },
    [ACTION_SHOW_WINDOW_LIST] = { "SHOW-WINDOW-LIST", PARSER_DATA_TYPE_VOID },
    [ACTION_RUN] = { "RUN", PARSER_DATA_TYPE_STRING },
    [ACTION_SHOW_MESSAGE] = { "SHOW-MESSAGE", PARSER_DATA_TYPE_STRING },
    [ACTION_SHOW_MESSAGE_RUN] = { "SHOW-MESSAGE-RUN", PARSER_DATA_TYPE_STRING },
    [ACTION_RESIZE_BY] = { "RESIZE-BY", PARSER_DATA_TYPE_QUAD },
    [ACTION_QUIT] = { "QUIT", PARSER_DATA_TYPE_VOID },
};

/* Get the data type the action expects as parameter. */
parser_data_type_t get_action_data_type(action_t action)
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
const char *action_to_string(action_t action)
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
     * processes
     */

    /* create a child process */
    child_process_id = fork();
    if (child_process_id == 0) {
        /* this code is executed in the child */

        /* create a grandchild process */
        if (fork() == 0) {
            /* this code is executed in the grandchild process */
            execl("/bin/sh", "sh", "-c", shell, (char*) NULL);
            /* this point is not reached because `execl()` replaces the process
             */
        } else {
            _exit(0);
        }
    } else {
        /* wait until the child process exits */
        waitpid(child_process_id, NULL, 0);
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
static void resize_frame_or_window_by(Window *window, int32_t left, int32_t top,
        int32_t right, int32_t bottom)
{
    Frame *frame;

    if (window == NULL) {
        frame = focus_frame;
        if (frame == NULL) {
            return;
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
}

/* Get a tiling window that is not currently shown and put it into the focus
 * frame.
 */
void set_showable_tiling_window(bool previous)
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

    if (valid_window != NULL) {
        show_window(valid_window);
        set_focus_window(valid_window);
    }
}

/* Change the focus from tiling to non tiling or vise versa. */
void toggle_focus(void)
{
    Window *window;

    if (focus_window == NULL ||
            focus_window->state.mode == WINDOW_MODE_TILING) {
        /* the the first window on the Z stack that is visible */
        for (window = top_window; window != NULL; window = window->below) {
            if (window->state.mode == WINDOW_MODE_TILING) {
                window = NULL;
                break;
            }
            if (window->state.is_visible) {
                break;
            }
        }

        if (window != NULL) {
            set_focus_window_with_frame(window);
        }
    } else {
        set_focus_frame(focus_frame);
    }
}

/* Do the given action. */
void do_action(const Action *action, Window *window)
{
    Frame *frame;
    char *shell;

    switch (action->code) {
    /* invalid action value */
    case ACTION_NULL:
        LOG_ERROR("tried to do NULL action");
        break;

    /* do nothing */
    case ACTION_NONE:
        break;

    /* reload the configuration file */
    case ACTION_RELOAD_CONFIGURATION:
        /* this needs to be delayed because if this is called by a binding and
         * the configuration is immediately reloaded, the pointer to the binding
         * becomes invalid and a crash occurs
         */
        is_reload_requested = true;
        break;

    /* move the focus to the parent frame */
    case ACTION_PARENT:
        if (focus_frame->parent != NULL) {
            focus_frame = focus_frame->parent;
        }
        set_focus_frame(focus_frame);
        break;

    /* move the focus to the child frame */
    case ACTION_CHILD:
        if (focus_frame->left != NULL) {
            focus_frame = focus_frame->left;
        }
        set_focus_frame(focus_frame);
        break;

    /* closes the currently active window */
    case ACTION_CLOSE_WINDOW:
        if (window == NULL) {
            break;
        }
        close_window(window);
        break;

    /* hide the currently active window */
    case ACTION_MINIMIZE_WINDOW:
        if (window == NULL) {
            break;
        }
        hide_window(window);
        break;

    /* focus a window */
    case ACTION_FOCUS_WINDOW:
        set_focus_window(window);
        if (window != NULL) {
            update_window_layer(window);
        }
        break;

    /* start moving a window with the mouse */
    case ACTION_INITIATE_MOVE:
        if (window == NULL) {
            break;
        }
        initiate_window_move_resize(window, _NET_WM_MOVERESIZE_MOVE, -1, -1);
        break;

    /* start resizing a window with the mouse */
    case ACTION_INITIATE_RESIZE:
        if (window == NULL) {
            break;
        }
        initiate_window_move_resize(window, _NET_WM_MOVERESIZE_AUTO, -1, -1);
        break;

    /* go to the next window in the window list */
    case ACTION_NEXT_WINDOW:
        set_showable_tiling_window(false);
        break;

    /* go to the previous window in the window list */
    case ACTION_PREVIOUS_WINDOW:
        set_showable_tiling_window(true);
        break;

    /* remove the current frame */
    case ACTION_REMOVE_FRAME:
        (void) stash_frame(focus_frame);
        (void) remove_void(focus_frame);
        break;

    /* changes a non tiling window to a tiling window and vise versa */
    case ACTION_TOGGLE_TILING:
        if (window == NULL) {
            break;
        }
        set_window_mode(window,
                window->state.mode == WINDOW_MODE_TILING ?
                WINDOW_MODE_FLOATING : WINDOW_MODE_TILING);
        break;

    /* toggles the fullscreen state of the currently focused window */
    case ACTION_TOGGLE_FULLSCREEN:
        if (window != NULL) {
            set_window_mode(window,
                    window->state.mode == WINDOW_MODE_FULLSCREEN ?
                    window->state.previous_mode : WINDOW_MODE_FULLSCREEN);
        }
        break;

    /* change the focus from tiling to non tiling or vise versa */
    case ACTION_TOGGLE_FOCUS:
        toggle_focus();
        break;

    /* split the current frame horizontally */
    case ACTION_SPLIT_HORIZONTALLY:
        split_frame(focus_frame, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_VERTICALLY:
        split_frame(focus_frame, FRAME_SPLIT_VERTICALLY);
        break;

    /* move to the frame above the current one */
    case ACTION_MOVE_UP:
        frame = get_frame_at_position(focus_frame->x, focus_frame->y - 1);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame left of the current one */
    case ACTION_MOVE_LEFT:
        frame = get_frame_at_position(focus_frame->x - 1, focus_frame->y);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame right of the current one */
    /* TODO: for all below, do not use get_frame_at_position() */
    case ACTION_MOVE_RIGHT:
        frame = get_frame_at_position(focus_frame->x + focus_frame->width, focus_frame->y);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* move to the frame below the current one */
    case ACTION_MOVE_DOWN:
        frame = get_frame_at_position(focus_frame->x, focus_frame->y + focus_frame->height);
        if (frame != NULL) {
            set_focus_frame(frame);
        }
        break;

    /* toggle visibility of the interactive window list */
    case ACTION_SHOW_WINDOW_LIST:
        if (show_window_list() == ERROR) {
            unmap_client(&window_list.client);
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
        set_notification((utf8_t*) shell,
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        free(shell);
        break;

    /* resize the edges of the current window */
    case ACTION_RESIZE_BY:
        resize_frame_or_window_by(window, action->parameter.quad[0],
                action->parameter.quad[1],
                action->parameter.quad[2],
                action->parameter.quad[3]);
        break;

    /* not a real action */
    case ACTION_MAX:
        break;
    }
}
