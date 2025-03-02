#ifndef WINDOW_LIST_H
#define WINDOW_LIST_H

#include "window.h"

/* user window list window */
extern struct window_list {
    /* the X correspondence */
    XClient client;
    /* the currently selected window */
    Window *selected;
    /* the currently scrolled amount */
    uint32_t vertical_scrolling;
    /* if the focus should return when the window list gets unmapped */
    bool should_revert_focus;
} window_list;

/* Create the window list. */
int initialize_window_list(void);

/* Handle an incoming event for the window list. */
void handle_window_list_event(xcb_generic_event_t *event);

/* Show the window list on screen.
 *
 * The user can now select a window and it will be shown on screen.
 *
 * @return ERROR if the window list can not be shown, OK otherwise.
 *         Either it is already shown or there are no windows to select from.
 */
int show_window_list(void);

#endif
