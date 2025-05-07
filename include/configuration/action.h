#ifndef CONFIGURATION__ACTION_H
#define CONFIGURATION__ACTION_H

/**
 * Actions expose internal functionality to the user.
 *
 * The user can invoke any actions in any order at any time.
 */

#include "configuration/data_type.h"

/* This expands to all actions.  Action strings with equal prefix should come
 * after each other for higher efficiency while parsing.
 *
 * Action strings are formatted like this:
 * - The action string consists of words separated by a single space
 * - I is an integer
 * - S is a string
 */
#define DEFINE_ALL_PARSE_ACTIONS \
    /* assign a number to a frame */ \
    X(ACTION_ASSIGN, "assign I") \
    /* assign a number to a window */ \
    X(ACTION_ASSIGN_WINDOW, "assign window I") \
    /* automatically equalize the frames when a frame is split or removed */ \
    X(ACTION_AUTO_EQUALIZE, "auto equalize I") \
    /* automatic filling of voids */ \
    X(ACTION_AUTO_FILL_VOID, "auto fill void I") \
    /* automatic removal of windows (implies remove void) */ \
    X(ACTION_AUTO_REMOVE, "auto remove I") \
    /* automatic removal of voids */ \
    X(ACTION_AUTO_REMOVE_VOID, "auto remove void I") \
    /* automatic splitting */ \
    X(ACTION_AUTO_SPLIT, "auto split I") \
    /* the background color of the fensterchef windows */ \
    X(ACTION_BACKGROUND, "background I") \
    /* the border color of "active" windows */ \
    X(ACTION_BORDER_COLOR_ACTIVE, "border color active I") \
    /* the border color of focused windows */ \
    X(ACTION_BORDER_COLOR_FOCUS, "border color focus I") \
    /* the border color of all windows */ \
    X(ACTION_BORDER_COLOR, "border color I") \
    /* the border size of all windows */ \
    X(ACTION_BORDER_SIZE, "border size I") \
    /* center the window to the monitor it is on */ \
    X(ACTION_CENTER_WINDOW, "center window") \
    /* center a window to given monitor (glob pattern) */ \
    X(ACTION_CENTER_WINDOW_TO, "center window to S") \
    /* closes the currently active window */ \
    X(ACTION_CLOSE_WINDOW, "close window") \
    /* closes the window with given number */ \
    X(ACTION_CLOSE_WINDOW_I, "close window I") \
    /* set the default cursor for horizontal sizing */ \
    X(ACTION_CURSOR_HORIZONTAL, "cursor horizontal S") \
    /* set the default cursor for movement */ \
    X(ACTION_CURSOR_MOVING, "cursor moving S") \
    /* set the default root cursor */ \
    X(ACTION_CURSOR_ROOT, "cursor root S") \
    /* set the default cursor for sizing a corner */ \
    X(ACTION_CURSOR_SIZING, "cursor sizing S") \
    /* set the default cursor for vertical sizing */ \
    X(ACTION_CURSOR_VERTICAL, "cursor vertical S") \
    /* write all fensterchef information to a file */ \
    X(ACTION_DUMP_LAYOUT, "dump layout S") \
    /* equalize the size of the child frames within the current frame */ \
    X(ACTION_EQUALIZE, "equalize") \
    /* exchange the current frame with the below one */ \
    X(ACTION_EXCHANGE_DOWN, "exchange down") \
    /* exchange the current frame with the left one */ \
    X(ACTION_EXCHANGE_LEFT, "exchange left") \
    /* exchange the current frame with the right one */ \
    X(ACTION_EXCHANGE_RIGHT, "exchange right") \
    /* exchange the current frame with the above one */ \
    X(ACTION_EXCHANGE_UP, "exchange up") \
    /* the number of the first window */ \
    X(ACTION_FIRST_WINDOW_NUMBER, "first window number I") \
    /* focus the child of the current frame */ \
    X(ACTION_FOCUS_CHILD, "focus child") \
    /* focus the ith child of the current frame */ \
    X(ACTION_FOCUS_CHILD_I, "focus child I") \
    /* focus the frame below */ \
    X(ACTION_FOCUS_DOWN, "focus down") \
    /* focus a frame with given number or the window within the frame */ \
    X(ACTION_FOCUS, "focus I") \
    /* move the focus to the leaf frame */ \
    X(ACTION_FOCUS_LEAF, "focus leaf") \
    /* move the focus to the left frame */ \
    X(ACTION_FOCUS_LEFT, "focus left") \
    /* move the focus to the parent frame */ \
    X(ACTION_FOCUS_PARENT, "focus parent") \
    /* move the focus to the ith parent frame */ \
    X(ACTION_FOCUS_PARENT_I, "focus parent I") \
    /* move the focus to the right frame */ \
    X(ACTION_FOCUS_RIGHT, "focus right") \
    /* move the focus to the root frame */ \
    X(ACTION_FOCUS_ROOT, "focus root") \
    /* move the focus to the root frame of given monitor */ \
    X(ACTION_FOCUS_ROOT_S, "focus root S") \
    /* move the focus to the above frame */ \
    X(ACTION_FOCUS_UP, "focus up") \
    /* refocus the current window */ \
    X(ACTION_FOCUS_WINDOW, "focus window") \
    /* focus the window with given number */ \
    X(ACTION_FOCUS_WINDOW_I, "focus window I") \
    /* the font used for rendering */ \
    X(ACTION_FONT, "font S") \
    /* the foreground color of the fensterchef windows */ \
    X(ACTION_FOREGROUND, "foreground I") \
    /* the inner gaps between frames and windows */ \
    X(ACTION_GAPS_INNER, "gaps inner I") \
    /* set the horizontal and vertical inner gaps */ \
    X(ACTION_GAPS_INNER_I_I, "gaps inner I I") \
    /* set the left, right, top and bottom inner gaps */ \
    X(ACTION_GAPS_INNER_I_I_I_I, "gaps inner I I I I") \
    /* the outer gaps between frames and monitors */ \
    X(ACTION_GAPS_OUTER, "gaps outer I") \
    /* set the horizontal and vertical outer gaps */ \
    X(ACTION_GAPS_OUTER_I_I, "gaps outer I I") \
    /* set the left, right, top and bottom outer gaps */ \
    X(ACTION_GAPS_OUTER_I_I_I_I, "gaps outer I I I I") \
    /* hint that the current frame should split horizontally */ \
    X(ACTION_HINT_SPLIT_HORIZONTALLY, "hint split horizontally") \
    /* hint that the current frame should split vertically */ \
    X(ACTION_HINT_SPLIT_VERTICALLY, "hint split vertically") \
    /* start moving a window with the mouse */ \
    X(ACTION_INITIATE_MOVE, "initiate move") \
    /* start resizing a window with the mouse */ \
    X(ACTION_INITIATE_RESIZE, "initiate resize") \
    /* merge in the default settings */ \
    X(ACTION_MERGE_DEFAULT, "merge default") \
    /* hide currently active window */ \
    X(ACTION_MINIMIZE_WINDOW, "minimize window") \
    /* hide the window with given number */ \
    X(ACTION_MINIMIZE_WINDOW_I, "minimize window I") \
    /* the modifiers to use for the following bindings */ \
    X(ACTION_MODIFIERS, "modifiers I") \
    /* the modifiers to ignore */ \
    X(ACTION_MODIFIERS_IGNORE, "modifiers ignore I") \
    /* move the current frame down */ \
    X(ACTION_MOVE_DOWN, "move down") \
    /* move the current frame to the left */ \
    X(ACTION_MOVE_LEFT, "move left") \
    /* move the current frame to the right */ \
    X(ACTION_MOVE_RIGHT, "move right") \
    /* move the current frame up */ \
    X(ACTION_MOVE_UP, "move up") \
    /* resize the edges of the current window by given values */ \
    X(ACTION_MOVE_WINDOW_BY, "move window by I I") \
    /* move the position of the current window to given position */ \
    X(ACTION_MOVE_WINDOW_TO, "move window to I I") \
    /* the duration the notification window stays for */ \
    X(ACTION_NOTIFICATION_DURATION, "notification duration I") \
    /* the value at which a window should be counted as overlapping a monitor */ \
    X(ACTION_OVERLAP, "overlap I") \
    /* replace the current frame with a frame from the stash */ \
    X(ACTION_POP_STASH, "pop stash") \
    /* quit fensterchef */ \
    X(ACTION_QUIT, "quit") \
    /* reload the configuration file */ \
    X(ACTION_RELOAD_CONFIGURATION, "reload configuration") \
    /* remove the current frame */ \
    X(ACTION_REMOVE, "remove") \
    /* remove frame with given number */ \
    X(ACTION_REMOVE_I, "remove I") \
    /* resize the current window by given values */ \
    X(ACTION_RESIZE_WINDOW_BY, "resize window by I I") \
    /* resize the current window to given values */ \
    X(ACTION_RESIZE_WINDOW_TO, "resize window to I I") \
    /* run a shell program */ \
    X(ACTION_RUN, "run S") \
    /* set the mode of the current window to floating */ \
    X(ACTION_SET_FLOATING, "set floating") \
    /* set the mode of the current window to fullscreen */ \
    X(ACTION_SET_FULLSCREEN, "set fullscreen") \
    /* set the mode of the current window to tiling */ \
    X(ACTION_SET_TILING, "set tiling") \
    /* show the interactive window list */ \
    X(ACTION_SHOW_LIST, "show list") \
    /* show a notification with a string message */ \
    X(ACTION_SHOW_MESSAGE, "show message S") \
    /* go to the next window in the window list */ \
    X(ACTION_SHOW_NEXT_WINDOW, "show next window") \
    /* go to the ith next window in the window list */ \
    X(ACTION_SHOW_NEXT_WINDOW_I, "show next window I") \
    /* go to the previous window in the window list */ \
    X(ACTION_SHOW_PREVIOUS_WINDOW, "show previous window") \
    /* go to the previous window in the window list */ \
    X(ACTION_SHOW_PREVIOUS_WINDOW_I, "show previous window I") \
    /* show a notification with a message extracted from a shell program */ \
    X(ACTION_SHOW_RUN, "show run S") \
    /* show the currently active widnow */ \
    X(ACTION_SHOW_WINDOW, "show window") \
    /* show the window with given number */ \
    X(ACTION_SHOW_WINDOW_I, "show window I") \
    /* split the current frame horizontally */ \
    X(ACTION_SPLIT_HORIZONTALLY, "split horizontally") \
    /* split the current frame left horizontally */ \
    X(ACTION_SPLIT_LEFT_HORIZONTALLY, "split left horizontally") \
    /* split the current frame left vertically */ \
    X(ACTION_SPLIT_LEFT_VERTICALLY, "split left vertically") \
    /* split the current frame vertically */ \
    X(ACTION_SPLIT_VERTICALLY, "split vertically") \
    /* the text padding within the fensterchef windows */ \
    X(ACTION_TEXT_PADDING, "text padding I") \
    /* change the focus from tiling to non tiling or vise versa */ \
    X(ACTION_TOGGLE_FOCUS, "toggle focus") \
    /* toggles the fullscreen state of the currently focused window */ \
    X(ACTION_TOGGLE_FULLSCREEN, "toggle fullscreen") \
    /* changes a non tiling window to a tiling window and vise versa */ \
    X(ACTION_TOGGLE_TILING, "toggle tiling")

/* action codes */
typedef enum {
#define X(identifier, string) \
    identifier,
    DEFINE_ALL_PARSE_ACTIONS
#undef X
    /* not a real action */
    ACTION_MAX,
} action_type_t;

/* A list of actions. */
struct action_list {
    /* all items within the list */
    struct action_list_item {
        /* the type of this actions */
        action_type_t type;
        /* the number of data points in `data` */
        unsigned data_count;
    } *items;
    /* the number of items in `items` */
    size_t number_of_items;
    /* the data associated to the actions */
    struct parse_generic_data *data;
};

/* Do all actions within @list. */
void do_action_list(const struct action_list *list);

/* Free very deep memory associated to the action list @list. */
void clear_action_list(struct action_list *list);

/* Free ALL memory associated to the action list @list. */
void clear_action_list_deeply(struct action_list *list);

/* Do the given action using given @data. */
void do_action(action_type_t type, const struct parse_generic_data *data);

#endif
