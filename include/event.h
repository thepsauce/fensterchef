#ifndef EVENT_H
#define EVENT_H

#include "bits/window_typedef.h"

#include "x11_management.h"

/* this is the first index of a randr event */
extern uint8_t randr_event_base;

/* if the user requested to reload the configuration */
extern bool is_reload_requested;

/* if the client list has changed (if stacking changed, windows were removed or
 * added)
 */
extern bool has_client_list_changed;

/* Create a signal handler for `SIGALRM`. */
int initialize_signal_handlers(void);

/* Runs the next cycle of the event loop. This handles signals and all events
 * that are currently queued.
 *
 * It also delegates events to the window list if it is mapped.
 */
int next_cycle(void);

/* Start resizing a window using the mouse. */
void initiate_window_move_resize(Window *window,
        wm_move_resize_direction_t direction,
        int32_t start_x, int32_t start_y);

/* Handle the given X event. */
void handle_event(xcb_generic_event_t *event);

#endif
