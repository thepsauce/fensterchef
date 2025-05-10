#include <errno.h>
#include <inttypes.h>
#include <string.h> /* strcmp() */

#include "action.h"
#include "binding.h"
#include "cursor.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "frame_moving.h"
#include "frame_sizing.h"
#include "frame_splitting.h"
#include "frame_stashing.h"
#include "log.h"
#include "monitor.h"
#include "notification.h"
#include "parse/data_type.h"
#include "parse/parse.h"
#include "parse/stream.h"
#include "window_list.h"
#include "window_stacking.h"
#include "x11_synchronize.h"

/* the corresponding string identifier for all actions */
static const char *action_strings[ACTION_MAX] = {
#define X(identifier, string) \
    [identifier] = string,
    DEFINE_ALL_PARSE_ACTIONS
#undef X
};

/* Get the action string of given action type. */
inline const char *get_action_string(action_type_t type)
{
    return action_strings[type];
}

static void free_action_list(struct action_list *list)
{
    clear_action_list_but_keep_shallow(list);
    free(list->items);
    free(list->data);
}

/* Do all actions within @list. */
void run_action_list(const struct action_list *original_list)
{
    struct action_list list;
    struct action_list_item *item;
    struct parse_generic_data *data;

    /* make a copy so we need to to worry about its origin and whether the next
     * action will invalidate the pointer or items within it
     */
    list = *original_list;
    duplicate_action_list(&list);

    data = list.data;
    for (unsigned i = 0; i < list.number_of_items; i++) {
        item = &list.items[i];
        do_action(item->type, data);
        data += item->data_count;
    }

    clear_action_list(&list);
}

/* Make a deep copy of @list and put it into itself. */
void duplicate_action_list(struct action_list *list)
{
    struct action_list_item *item;
    struct parse_generic_data *data;
    size_t data_count = 0;

    for (unsigned i = 0; i < list->number_of_items; i++) {
        item = &list->items[i];
        data_count += item->data_count;
    }

    list->items = DUPLICATE(list->items, list->number_of_items);
    list->data = DUPLICATE(list->data, data_count);

    data = list->data;
    for (size_t i = 0; i < data_count; i++) {
        switch (data->type) {
        case PARSE_DATA_TYPE_INTEGER:
            /* nothing */
            break;

        case PARSE_DATA_TYPE_STRING:
            data->u.string = xstrdup(data->u.string);
            break;

        case PARSE_DATA_TYPE_BUTTON:
            duplicate_action_list(&data->u.button.actions);
            break;

        case PARSE_DATA_TYPE_KEY:
            duplicate_action_list(&data->u.key.actions);
            break;
        }
        data++;
    }
}

/* Free deep memory associated to the action list @list. */
void clear_action_list_but_keep_shallow(struct action_list *list)
{
    struct parse_generic_data *data;

    data = list->data;
    for (unsigned i = 0; i < list->number_of_items; i++) {
        for (unsigned j = 0; j < list->items[i].data_count; j++) {
            switch (data->type) {
            case PARSE_DATA_TYPE_INTEGER:
                /* nothing */
                break;

            case PARSE_DATA_TYPE_STRING:
                free(data->u.string);
                break;

            case PARSE_DATA_TYPE_BUTTON:
                free_action_list(&data->u.button.actions);
                break;

            case PARSE_DATA_TYPE_KEY:
                free_action_list(&data->u.key.actions);
                break;
            }
            data++;
        }
    }
}

/* Free ALL memory associated to the action list @list and set it to 0. */
void clear_action_list(struct action_list *list)
{
    free_action_list(list);
    list->items = NULL;
    list->number_of_items = 0;
    list->data = NULL;
}

/* Log a list of actions to stderr. */
void log_action_list(const struct action_list *list)
{
    const struct action_list_item *item;
    const struct parse_generic_data *data, *previous_data;

    data = list->data;
    for (size_t i = 0; i < list->number_of_items; i++) {
        const char *action, *space;
        int length;

        if (i > 0) {
            fputs(CLEAR_COLOR ", ", stderr);
        }

        item = &list->items[i];
        if (item->type == ACTION_BUTTON_BINDING) {
            const struct button_binding *const binding = &data->u.button;
            if (binding->is_release) {
                fprintf(stderr, COLOR(YELLOW) "release ");
            }
            if (binding->is_transparent) {
                fprintf(stderr, COLOR(YELLOW) "transparent ");
            }
            if (binding->modifiers != 0) {
                fprintf(stderr, COLOR(BLUE) "%u" CLEAR_COLOR "+",
                        binding->modifiers);
            }
            _log_formatted("%u %A",
                    binding->modifiers, &binding->actions);
            data++;
            continue;
        } else if (item->type == ACTION_KEY_BINDING) {
            const struct key_binding *const binding = &data->u.key;
            if (binding->is_release) {
                fprintf(stderr, COLOR(YELLOW) "release ");
            }
            if (binding->modifiers != 0) {
                fprintf(stderr, COLOR(BLUE) "%u" CLEAR_COLOR "+",
                        binding->modifiers);
            }
            _log_formatted("%ld %A",
                    binding->key_symbol, &binding->actions);
            data++;
            continue;
        }
        action = action_strings[item->type];
        previous_data = data;
        while (action != NULL) {
            space = strchr(action, ' ');
            if (space != NULL) {
                length = space - action;
                space++;
            } else {
                length = strlen(action);
            }

            /* check if this is after the first part of the action */
            if (action != action_strings[item->type]) {
                fputs(" ", stderr);
            }

            if (length == 1 && action[0] == 'S') {
                fprintf(stderr, COLOR(GREEN) "%s",
                        data->u.string);
                data++;
            } else if (length == 1 && action[0] == 'I') {
                fprintf(stderr, COLOR(GREEN) "%" PRIiPARSE_INTEGER,
                        data->u.integer);
                data++;
            } else {
                fprintf(stderr, COLOR(YELLOW) "%.*s",
                        length, action);
            }

            action = space;
        }
        data = previous_data + item->data_count;
    }

    fputs(CLEAR_COLOR, stderr);
}

/* Resize the current window or current frame if it does not exist. */
static bool resize_frame_or_window_by(FcWindow *window, int left, int top,
        int right, int bottom)
{
    Frame *frame;

    if (window == NULL) {
        frame = Frame_focus;
        if (frame == NULL) {
            return false;
        }
    } else {
        frame = get_window_frame(window);
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
        if ((int) window->width < -right) {
            right = -window->width;
        }
        if ((int) window->height < -bottom) {
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
bool set_showable_tiling_window(unsigned count, bool previous)
{
    FcWindow *start, *next, *valid_window = NULL;

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
        set_system_notification("No other window",
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
    FcWindow *window;

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
static void move_to_frame(Frame *from, Frame *to, bool do_exchange)
{
    if (do_exchange) {
        exchange_frames(from, to);
    } else {
        /* simply "move" to the next frame by focusing it */
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
    move_to_frame(relative, frame, do_exchange);
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
    move_to_frame(relative, frame, do_exchange);
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
    move_to_frame(relative, frame, do_exchange);
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
    move_to_frame(relative, frame, do_exchange);
    return true;
}

/* Translate the integer data if not using pixels. */
static inline int translate_integer_data(Monitor *monitor,
        const struct parse_generic_data *data, bool is_x_axis)
{
    if ((data->flags & PARSE_DATA_FLAGS_IS_PERCENT)) {
        unsigned size;

        if (is_x_axis) {
            size = monitor->width;
        } else {
            size = monitor->height;
        }
        return size * (unsigned) data->u.integer / 100;
    }

    return data->u.integer;
}

/* Do the given action. */
void do_action(action_type_t type, const struct parse_generic_data *data)
{
    FcWindow *window;
    char *shell;
    int count = 1;
    Frame *frame;
    bool is_previous = true;

    window = Window_pressed;
    if (window == NULL) {
        window = Window_focus;
    }

    frame = Frame_focus;

    switch (type) {
    /* assign a number to a frame */
    case ACTION_ASSIGN:
        /* remove the number from the old frame if there is any */
        frame = get_frame_by_number((unsigned) data->u.integer);
        /* also try to find it in the stash */
        if (frame == NULL) {
            frame = Frame_last_stashed;
            for (; frame != NULL; frame = frame->previous_stashed) {
                if (frame->number == (unsigned) data->u.integer) {
                    break;
                }
            }
        }
        if (frame != NULL) {
            frame->number = 0;
        }

        Frame_focus->number = data->u.integer;
        if (Frame_focus->number == 0) {
            set_system_notification("Number removed",
                    Frame_focus->x + Frame_focus->width / 2,
                    Frame_focus->y + Frame_focus->height / 2);
        } else {
            char number[MAXIMUM_DIGITS(Frame_focus->number) + 1];

            snprintf(number, sizeof(number), "%" PRIu32, Frame_focus->number);
            set_system_notification(number,
                    Frame_focus->x + Frame_focus->width / 2,
                    Frame_focus->y + Frame_focus->height / 2);
        }
        break;

    /* assign a number to a window */
    case ACTION_ASSIGN_WINDOW:
        if (window == NULL) {
            break;
        }
        window->number = data->u.integer;
        break;

    /* automatically equalize the frames when a frame is split or removed */
    case ACTION_AUTO_EQUALIZE:
        configuration.auto_equalize = data->u.integer;
        break;

    /* automatic filling of voids */
    case ACTION_AUTO_FILL_VOID:
        configuration.auto_fill_void = data->u.integer;
        break;

    /* automatic removal of windows (implies remove void) */
    case ACTION_AUTO_REMOVE:
        configuration.auto_remove = data->u.integer;
        break;

    /* automatic removal of voids */
    case ACTION_AUTO_REMOVE_VOID:
        configuration.auto_remove_void = data->u.integer;
        break;

    /* automatic splitting */
    case ACTION_AUTO_SPLIT:
        configuration.auto_split = data->u.integer;
        break;

    /* the background color of the fensterchef windows */
    case ACTION_BACKGROUND:
        configuration.background = data->u.integer;
        break;

    /* the border color of all windows */
    case ACTION_BORDER_COLOR:
        configuration.border_color = data->u.integer;
        break;

    /* the border color of "active" windows */
    case ACTION_BORDER_COLOR_ACTIVE:
        configuration.border_color_active = data->u.integer;
        break;

    /* the border color of focused windows */
    case ACTION_BORDER_COLOR_FOCUS:
        configuration.border_color_focus = data->u.integer;
        break;

    /* the border size of all windows */
    case ACTION_BORDER_SIZE:
        configuration.border_size = data->u.integer;
        break;

    /* center a window to the current monitor */
    case ACTION_CENTER_WINDOW: {
        Monitor *monitor;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_containing_window(window);
        set_window_size(window,
                monitor->x + (monitor->width - window->width) / 2,
                monitor->y + (monitor->height - window->height) / 2,
                window->width, window->height);
        break;
    }

    /* center a window to given monitor (glob pattern) */
    case ACTION_CENTER_WINDOW_TO: {
        Monitor *monitor;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_by_pattern(data->u.string);

        if (monitor == NULL) {
            LOG_ERROR("no monitor matches %s\n",
                    data->u.string);
            break;
        }

        set_window_size(window,
                monitor->x + (monitor->width - window->width) / 2,
                monitor->y + (monitor->height - window->height) / 2,
                window->width, window->height);
        break;
    }

    /* closes the window with given number */
    case ACTION_CLOSE_WINDOW_I:
        window = get_window_by_number(data->u.integer);
        /* fall through */
    /* closes the currently active window */
    case ACTION_CLOSE_WINDOW:
        if (window == NULL) {
            break;
        }
        close_window(window);
        break;

    /* set the default cursor for horizontal sizing */
    case ACTION_CURSOR_HORIZONTAL:
        (void) load_cursor(CURSOR_HORIZONTAL, data->u.string);
        break;

    /* set the default cursor for movement */
    case ACTION_CURSOR_MOVING:
        (void) load_cursor(CURSOR_MOVING, data->u.string);
        break;

    /* set the default root cursor */
    case ACTION_CURSOR_ROOT:
        (void) load_cursor(CURSOR_ROOT, data->u.string);
        synchronization_flags |= SYNCHRONIZE_ROOT_CURSOR;
        break;

    /* set the default cursor for vertical sizing */
    case ACTION_CURSOR_SIZING:
        (void) load_cursor(CURSOR_VERTICAL, data->u.string);
        break;

    /* set the default cursor for vertical sizing */
    case ACTION_CURSOR_VERTICAL:
        (void) load_cursor(CURSOR_VERTICAL, data->u.string);
        break;

    /* write all frensterchef information to a file */
    case ACTION_DUMP_LAYOUT:
        if (dump_frames_and_windows(data->u.string) == ERROR) {
            LOG_ERROR("can not write dump to %s: %s\n",
                    data->u.string, strerror(errno));
            break;
        }
        break;

    /* equalize the size of the child frames within a frame */
    case ACTION_EQUALIZE:
        equalize_frame(Frame_focus, FRAME_SPLIT_HORIZONTALLY);
        equalize_frame(Frame_focus, FRAME_SPLIT_VERTICALLY);
        break;

    /* exchange the current frame with the below one */
    case ACTION_EXCHANGE_DOWN:
        move_to_below_frame(Frame_focus, true);
        break;

    /* exchange the current frame with the left one */
    case ACTION_EXCHANGE_LEFT:
        move_to_left_frame(Frame_focus, true);
        break;

    /* exchange the current frame with the right one */
    case ACTION_EXCHANGE_RIGHT:
        move_to_right_frame(Frame_focus, true);
        break;

    /* exchange the current frame with the above one */
    case ACTION_EXCHANGE_UP:
        move_to_above_frame(Frame_focus, true);
        break;

    /* the number of the first window */
    case ACTION_FIRST_WINDOW_NUMBER:
        configuration.first_window_number = data->u.integer;
        break;

    /* focus a frame with given number */
    case ACTION_FOCUS:
        frame = get_frame_by_number((unsigned) data->u.integer);
        /* check if the frame is already shown */
        if (frame != NULL) {
            set_focus_frame(frame);
            break;
        }

        frame = Frame_last_stashed;
        /* also try to find it in the stash */
        for (; frame != NULL; frame = frame->previous_stashed) {
            if (frame->number == (unsigned) data->u.integer) {
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

    /* move the focus to the child frame */
    case ACTION_FOCUS_CHILD:
        if (Frame_focus->left != NULL) {
            if (Frame_focus->moved_from_left) {
                Frame_focus = Frame_focus->left;
            } else {
                Frame_focus = Frame_focus->right;
            }
        }
        break;

    /* focus the ith child of the current frame */
    case ACTION_FOCUS_CHILD_I:
        count = data->u.integer;
        if (count < 0) {
            count = INT32_MAX;
        }
        for (frame = Frame_focus; frame->left != NULL && count > 0; count--) {
            if (frame->moved_from_left) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        }
        Frame_focus = frame;
        break;

    /* move the focus to the frame below */
    case ACTION_FOCUS_DOWN:
        move_to_below_frame(Frame_focus, false);
        break;

    /* move the focus to the leaf frame */
    case ACTION_FOCUS_LEAF:
        /* move to the count'th child */
        for (frame = Frame_focus; frame->left != NULL; ) {
            if (frame->moved_from_left) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        }

        Frame_focus = frame;
        break;

    /* move the focus to the left frame */
    case ACTION_FOCUS_LEFT:
        move_to_left_frame(Frame_focus, false);
        break;

    /* move the focus to the parent frame */
    case ACTION_FOCUS_PARENT:
        if (Frame_focus->parent != NULL) {
            if (Frame_focus == Frame_focus->parent->left) {
                Frame_focus->parent->moved_from_left = true;
            } else {
                Frame_focus->parent->moved_from_left = false;
            }
            Frame_focus = Frame_focus->parent;
        }
        break;

    /* move the focus to the ith parent frame */
    case ACTION_FOCUS_PARENT_I:
        count = data->u.integer;
        if (count < 0) {
            count = INT32_MAX;
        }
        for (frame = Frame_focus; frame->parent != NULL && count > 0; count--) {
            if (frame == frame->parent->left) {
                frame->parent->moved_from_left = true;
            } else {
                frame->parent->moved_from_left = false;
            }
            frame = frame->parent;
        }
        Frame_focus = frame;
        break;


    /* move the focus to the right frame */
    case ACTION_FOCUS_RIGHT:
        move_to_right_frame(Frame_focus, false);
        break;

    /* move the focus to the root frame */
    case ACTION_FOCUS_ROOT:
        Frame_focus = get_root_frame(Frame_focus);
        break;

    /* move the focus to the root frame of given monitor */
    case ACTION_FOCUS_ROOT_S: {
        Monitor *monitor;

        monitor = get_monitor_by_pattern(data->u.string);
        if (monitor == NULL) {
            LOG_ERROR("no monitor matches %s\n",
                    data->u.string);
            break;
        }

        Frame_focus = monitor->frame;
        break;
    }

    /* move the focus to the frame above */
    case ACTION_FOCUS_UP:
        move_to_above_frame(Frame_focus, false);
        break;

    /* focus the window with given number */
    case ACTION_FOCUS_WINDOW_I:
        window = get_window_by_number(data->u.integer);
        /* fall through */
    /* refocus the current window */
    case ACTION_FOCUS_WINDOW:
        if (window == NULL || window == Window_focus) {
            break;
        }

        show_window(window);
        update_window_layer(window);
        set_focus_window_with_frame(window);
        break;

    /* the font used for rendering */
    case ACTION_FONT:
        set_font(data->u.string);
        break;

    /* the foreground color of the fensterchef windows */
    case ACTION_FOREGROUND:
        configuration.foreground = data->u.integer;
        break;

    /* the inner gaps between frames and windows */
    case ACTION_GAPS_INNER:
        configuration.gaps_inner[0] = data->u.integer;
        configuration.gaps_inner[1] = data->u.integer;
        configuration.gaps_inner[2] = data->u.integer;
        configuration.gaps_inner[3] = data->u.integer;
        break;

    /* set the horizontal and vertical inner gaps */
    case ACTION_GAPS_INNER_I_I:
        configuration.gaps_inner[0] = data[0].u.integer;
        configuration.gaps_inner[1] = data[1].u.integer;
        configuration.gaps_inner[2] = data[0].u.integer;
        configuration.gaps_inner[3] = data[1].u.integer;
        break;

    /* set the left, right, top and bottom inner gaps */
    case ACTION_GAPS_INNER_I_I_I_I:
        configuration.gaps_inner[0] = data[0].u.integer;
        configuration.gaps_inner[1] = data[1].u.integer;
        configuration.gaps_inner[2] = data[2].u.integer;
        configuration.gaps_inner[3] = data[3].u.integer;
        break;

    /* the outer gaps between frames and monitors */
    case ACTION_GAPS_OUTER:
        configuration.gaps_outer[0] = data->u.integer;
        configuration.gaps_outer[1] = data->u.integer;
        configuration.gaps_outer[2] = data->u.integer;
        configuration.gaps_outer[3] = data->u.integer;
        break;

    /* set the horizontal and vertical outer gaps */
    case ACTION_GAPS_OUTER_I_I:
        configuration.gaps_outer[0] = data[0].u.integer;
        configuration.gaps_outer[1] = data[1].u.integer;
        configuration.gaps_outer[2] = data[0].u.integer;
        configuration.gaps_outer[3] = data[1].u.integer;
        break;

    /* set the left, right, top and bottom outer gaps */
    case ACTION_GAPS_OUTER_I_I_I_I:
        configuration.gaps_outer[0] = data[0].u.integer;
        configuration.gaps_outer[1] = data[1].u.integer;
        configuration.gaps_outer[2] = data[2].u.integer;
        configuration.gaps_outer[3] = data[3].u.integer;
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

    /* merge in the default settings */
    case ACTION_MERGE_DEFAULT:
        merge_default_configuration(DEFAULT_CONFIGURATION_MERGE_ALL);
        break;

    /* hide the window with given number */
    case ACTION_MINIMIZE_WINDOW_I:
        window = get_window_by_number(data->u.integer);
        /* fall through */
    /* hide the currently active window */
    case ACTION_MINIMIZE_WINDOW:
        if (window == NULL || !window->state.is_visible) {
            break;
        }
        hide_window(window);
        break;

    /* the modifiers to use for the following bindings */
    case ACTION_MODIFIERS:
        set_additional_modifiers(data->u.integer);
        break;

    /* the modifiers to ignore */
    case ACTION_MODIFIERS_IGNORE:
        set_ignored_modifiers(data->u.integer);
        break;

    /* move the current frame down */
    case ACTION_MOVE_DOWN:
        move_frame_down(Frame_focus);
        break;

    /* move the current frame to the left */
    case ACTION_MOVE_LEFT:
        move_frame_left(Frame_focus);
        break;

    /* move the current frame to the right */
    case ACTION_MOVE_RIGHT:
        move_frame_right(Frame_focus);
        break;

    /* move the current frame up */
    case ACTION_MOVE_UP:
        move_frame_up(Frame_focus);
        break;

    /* move the current window */
    case ACTION_MOVE_WINDOW_BY: {
        Monitor *monitor;
        int x, y;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_containing_window(window);
        x = translate_integer_data(monitor, &data[0], true);
        y = translate_integer_data(monitor, &data[1], false);
        resize_frame_or_window_by(window, -x, -y, x, y);
        break;
    }

    /* move the current window relative to the current monitor */
    case ACTION_MOVE_WINDOW_TO: {
        Monitor *monitor;
        int x, y;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_containing_window(window);
        x = translate_integer_data(monitor, &data[0], true);
        y = translate_integer_data(monitor, &data[1], false);
        resize_frame_or_window_by(window,
                -(monitor->x + x - window->x), -(monitor->y + y - window->y),
                  monitor->x + x - window->x,    monitor->y + y - window->y);
        break;
    }

    /* the duration the notification window stays for */
    case ACTION_NOTIFICATION_DURATION:
        configuration.notification_duration = data->u.integer;
        break;

    /* the value at which a window should be counted as overlapping a monitor */
    case ACTION_OVERLAP:
        configuration.overlap = data->u.integer;
        break;

    /* remove the current frame and replace it with a frame from the stash */
    case ACTION_POP_STASH: {
        Frame *const pop = pop_stashed_frame();
        if (pop == NULL) {
            break;
        }
        stash_frame(Frame_focus);
        replace_frame(Frame_focus, pop);
        destroy_frame(pop);
        /* focus any window that might have appeared */
        set_focus_window(Frame_focus->window);
        break;
    }

    /* quit fensterchef */
    case ACTION_QUIT:
        Fensterchef_is_running = false;
        break;

    /* reload the configuration file */
    case ACTION_RELOAD_CONFIGURATION:
        reload_configuration();
        break;

    /* remove frame with given number */
    case ACTION_REMOVE_I:
        frame = get_frame_by_number(data->u.integer);
        if (frame == NULL) {
            break;
        }
        /* fall through */
    /* remove the current frame */
    case ACTION_REMOVE:
        (void) stash_frame(frame);
        /* remove the frame if it is not a root frame */
        if (frame->parent != NULL) {
            remove_frame(frame);
            destroy_frame(frame);
        }

        /* TODO: is this needed? */
        /* if nothing is focused/no longer focused, focus the window within the
         * current frame
         */
        if (Window_focus == NULL) {
            set_focus_window(Frame_focus->window);
        }
        break;

    /* resize the current window */
    case ACTION_RESIZE_WINDOW_BY: {
        Monitor *monitor;
        int width_change, height_change;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_containing_window(window);
        height_change = translate_integer_data(monitor, &data[0], true);
        width_change = translate_integer_data(monitor, &data[1], false);
        resize_frame_or_window_by(window, 0, 0, width_change, height_change);
        break;
    }

    /* resize the current window relative to the current monitor*/
    case ACTION_RESIZE_WINDOW_TO: {
        Monitor *monitor;
        int width, height;

        if (window == NULL) {
            break;
        }

        monitor = get_monitor_containing_window(window);
        width = translate_integer_data(monitor, &data[0], true);
        height = translate_integer_data(monitor, &data[1], false);
        resize_frame_or_window_by(window,
                0, 0,
                width - window->width,
                height - window->height);
        break;
    }

    /* run a shell program */
    case ACTION_RUN:
        run_shell(data->u.string);
        break;

    /* set the mode of the current window to floating */
    case ACTION_SET_FLOATING:
        if (window == NULL) {
            break;
        }
        set_window_mode(window, WINDOW_MODE_FLOATING);
        break;

    /* set the mode of the current window to fullscreen */
    case ACTION_SET_FULLSCREEN:
        if (window == NULL) {
            break;
        }
        set_window_mode(window, WINDOW_MODE_FULLSCREEN);
        break;

    /* set the mode of the current window to tiling */
    case ACTION_SET_TILING:
        if (window == NULL) {
            break;
        }
        set_window_mode(window, WINDOW_MODE_TILING);
        break;

    /* toggle visibility of the interactive window list */
    case ACTION_SHOW_LIST:
        if (show_window_list() == ERROR) {
            break;
        }
        break;

    /* show the user a message */
    case ACTION_SHOW_MESSAGE:
        set_system_notification(data->u.string,
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        break;

    /* go to the next window in the window list */
    case ACTION_SHOW_NEXT_WINDOW:
        is_previous = false;
        /* fall through */
    /* go to the previous window in the window list */
    case ACTION_SHOW_PREVIOUS_WINDOW:
        set_showable_tiling_window(1, is_previous);
        break;

    /* go to the ith next window in the window list */
    case ACTION_SHOW_NEXT_WINDOW_I:
        is_previous = false;
        /* fall through */
    /* go to the previous window in the window list */
    case ACTION_SHOW_PREVIOUS_WINDOW_I:
        count = data->u.integer;
        if (count < 0) {
            count *= -1;
            is_previous = !is_previous;
        }
        set_showable_tiling_window(count, is_previous);
        break;

    /* show a message by getting output from a shell program */
    case ACTION_SHOW_RUN:
        shell = run_shell_and_get_output(data->u.string);
        if (shell == NULL) {
            break;
        }
        set_system_notification(shell,
                Frame_focus->x + Frame_focus->width / 2,
                Frame_focus->y + Frame_focus->height / 2);
        free(shell);
        break;

    /* show the window with given number */
    case ACTION_SHOW_WINDOW_I:
        window = get_window_by_number(data->u.integer);
        /* fall through */
    /* show a window */
    case ACTION_SHOW_WINDOW:
        if (window == NULL || window->state.is_visible) {
            break;
        }

        show_window(window);
        update_window_layer(window);
        break;

    /* split the current frame horizontally */
    case ACTION_SPLIT_HORIZONTALLY:
        split_frame(Frame_focus, NULL, false, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame horizontally */
    case ACTION_SPLIT_LEFT_HORIZONTALLY:
        split_frame(Frame_focus, NULL, true, FRAME_SPLIT_HORIZONTALLY);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_LEFT_VERTICALLY:
        split_frame(Frame_focus, NULL, true, FRAME_SPLIT_VERTICALLY);
        break;

    /* split the current frame vertically */
    case ACTION_SPLIT_VERTICALLY:
        split_frame(Frame_focus, NULL, false, FRAME_SPLIT_VERTICALLY);
        break;

    /* the text padding within the fensterchef windows */
    case ACTION_TEXT_PADDING:
        configuration.text_padding = data->u.integer;
        break;

    /* change the focus from tiling to non tiling or vise versa */
    case ACTION_TOGGLE_FOCUS:
        toggle_focus();
        break;

    /* toggles the fullscreen state of the currently focused window */
    case ACTION_TOGGLE_FULLSCREEN:
        if (window == NULL) {
            break;
        }
        set_window_mode(window,
                window->state.mode == WINDOW_MODE_FULLSCREEN ?
                (window->state.previous_mode == WINDOW_MODE_FULLSCREEN ?
                    WINDOW_MODE_FLOATING : window->state.previous_mode) :
                WINDOW_MODE_FULLSCREEN);
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

    /* set a button binding */
    case ACTION_BUTTON_BINDING:
        set_button_binding(&data->u.button);
        break;

    /* set a key binding */
    case ACTION_KEY_BINDING:
        set_key_binding(&data->u.key);
        break;

    /* not a real action */
    case ACTION_MAX:
        break;
    }
}
