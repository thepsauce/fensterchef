#ifndef MONITOR_H
#define MONITOR_H

#include <xcb/randr.h>

#include "utility.h" // Position, Size

#include "bits/frame_typedef.h"

#include "x11_management.h"

/* A monitor is a rectangular region tied to a screen. */
typedef struct monitor {
    /* name of the monitor, used as key */
    char *name;

    /* if this is the primary monitor */
    unsigned primary : 1;

    /* temporary flag for merging */
    unsigned is_free : 1;

    /* region of the monitor to cut off */
    Extents strut;

    /* the position and size of the monitor */
    Position position;
    Size size;

    /* root frame */
    Frame *frame;

    /* next monitor in the linked list */
    struct monitor *next;
} Monitor;

/* the first monitor in the linked list */
extern Monitor *first_monitor;

/* Try to initialize randr and the internal monitor linked list. */
void initialize_monitors(void);

/* Get a monitor marked as primary or the first monitor if no monitor is marked
 * as primary.
 */
Monitor *get_primary_monitor(void);

/* Get the monitor that overlaps given rectangle the most. */
Monitor *get_monitor_from_rectangle(int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/* Gets a list of monitors that are associated to the screen.
 *
 * @return NULL when randr is not supported or when there are no monitors.
 */
Monitor *query_monitors(void);

/* Updates the struts of all monitors and then correctly sizes the frame. */
void reconfigure_monitor_frame_sizes(void);

/* Merges given monitor linked list into the screen's monitor list.
 *
 * @monitors may be NULL to indicate no monitors are there or randr is not
 *           supported.
 */
void merge_monitors(Monitor *monitors);

#endif
