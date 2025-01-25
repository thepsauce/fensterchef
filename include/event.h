#ifndef EVENT_H
#define EVENT_H

#include <xcb/xcb.h>

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event);

#endif
