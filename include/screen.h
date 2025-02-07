#ifndef SCREEN_H
#define SCREEN_H

#include <xcb/randr.h>

#include "util.h" // Position, Size

/* stock object indexes */
enum {
    STOCK_WHITE_PEN,
    STOCK_BLACK_PEN,

    STOCK_GC,
    STOCK_INVERTED_GC,

    STOCK_COUNT,
};

/* forward declaration */
struct monitor;

/* A screen is a user region that can contain multiple monitors. */
typedef struct screen {
    /* the screen number */
    int number;
    /* the underlying xcb screen */
    xcb_screen_t *xcb_screen;

    /* graphis objects with the id referring to the xcb id */
    uint32_t stock_objects[STOCK_COUNT];

    /* supporting wm check window */
    xcb_window_t check_window;
    /* user notification window */
    xcb_window_t notification_window;
    /* user window list window */
    xcb_window_t window_list_window;

    /* first monitor in the monitor linked list */
    struct monitor *monitor;
} Screen;

/* the actively used screen */
extern Screen *screen;

/* forward declaration */
struct frame;

/* A monitor is a rectangular region tied to a screen. */
typedef struct monitor {
    /* name of the monitor, used as key */
    char *name;

    /* if this is the primary monitor */
    unsigned primary : 1;

    /* temporary flag for merging */
    unsigned is_free : 1;

    /* region of the monitor to cut off */
    xcb_ewmh_get_extents_reply_t struts;

    /* the position and size of the monitor */
    Position position;
    Size size;

    /* root frame */
    struct frame *frame;

    /* next/prev monitor */
    struct monitor *prev;
    struct monitor *next;
} Monitor;

/* Initializes the window manager data of the screen. */
int init_screen(int screen_number);

/* Try to initialize randr for screen and monitor management. */
void init_monitors(void);

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
