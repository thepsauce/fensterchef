#ifndef EVENT_H
#define EVENT_H

#include <xcb/xcb.h>

/* this is the first index of a randr event */
extern uint8_t randr_event_base;

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event);

#endif
