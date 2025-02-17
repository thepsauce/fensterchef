#ifndef EVENT_H
#define EVENT_H

#include "bits/window_typedef.h"

#include "x11_management.h"

/* this is the first index of a randr event */
extern uint8_t randr_event_base;

/* Create a signal handler for `SIGALRM`. */
int initialize_signal_handlers(void);

/* called before processing any events or signals */
#define CYCLE_PREPARE 0

/* called before processing each X event */
#define CYCLE_EVENT 1

/* Runs the next cycle of the event loop. This handles signals and all events
 * that are currently queued.
 *
 * @callback is the function called before processing any events or signals:
 *      `(*callback)(CYCLE_PREPARE, NULL);`
 *      and then before any event is handled with `handle_event(event)`:
 *      `(*callback)(CYCLE_EVENT, event);`.
 */
int next_cycle(int (*callback)(int cycle_when, xcb_generic_event_t *event));

/* Start resizing a window using the mouse. */
void initiate_window_move_resize(Window *window,
        wm_move_resize_direction_t direction,
        int32_t start_x, int32_t start_y);

/* Handle the given X event. */
void handle_event(xcb_generic_event_t *event);

#endif
