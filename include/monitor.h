#ifndef MONITOR_H
#define MONITOR_H

#include "bits/frame.h"
#include "bits/monitor.h"
#include "x11_management.h"

/* A monitor is a rectangular region tied to a screen. */
struct monitor {
    /* name of the monitor, used as key */
    char *name;

    /* region of the monitor to cut off */
    Extents strut;

    /* the position and size of the monitor */
    int x;
    int y;
    unsigned width;
    unsigned height;

    /* root frame */
    Frame *frame;

    /* next monitor in the linked list */
    struct monitor *next;
};

/* the first monitor in the monitor linked list */
extern Monitor *Monitor_first;

/* Try to initialize randr and the internal monitor linked list. */
void initialize_monitors(void);

/* Go through all windows to find the total strut and apply it to all monitors.
 *
 * This also adjusts all dock windows so that they do not overlap.
 */
void reconfigure_monitor_frames(void);

/* The most efficient way to get the monitor containing given frame.
 *
 * @return NULL if the frame is not visible.
 */
Monitor *get_monitor_containing_frame(Frame *frame);

/* Get the monitor the window is on. */
Monitor *get_monitor_containing_window(FcWindow *window);

/* Get the monitor that overlaps given rectangle the most.
 *
 * @return NULL if no monitor intersects the rectangle at all.
 */
Monitor *get_monitor_from_rectangle(int x, int y,
        unsigned width, unsigned height);

/* Get the monitor that overlaps given rectangle the most or primary if there
 * are there are no intersections.
 */
Monitor *get_monitor_from_rectangle_or_primary(int x, int y,
        unsigned width, unsigned height);

/* Get a window covering given monitor. */
FcWindow *get_window_covering_monitor(Monitor *monitor);

/* Get the monitor on the left of @monitor.
 *
 * @return NULL if there is no monitor at the left.
 */
Monitor *get_left_monitor(Monitor *monitor);

/* Get the monitor above @monitor.
 *
 * @return NULL if there is no monitor above.
 */
Monitor *get_above_monitor(Monitor *monitor);

/* Get the monitor on the right of @monitor.
 *
 * @return NULL if there is no monitor at the right.
 */
Monitor *get_right_monitor(Monitor *monitor);

/* Get the monitor below of @monitor.
 *
 * @return NULL if there is no monitor below.
 */
Monitor *get_below_monitor(Monitor *monitor);

/* Get the first monitor matching given pattern. */
Monitor *get_monitor_by_pattern(const char *pattern);

/* Gets a list of monitors that are associated to the screen.
 *
 * @return NULL when randr is not supported or when there are no monitors.
 */
Monitor *query_monitors(void);

/* Merges given monitor linked list into the screen's monitor list.
 *
 * @monitors may be NULL to indicate no monitors are there or randr is not
 *           supported.
 */
void merge_monitors(Monitor *monitors);

#endif
