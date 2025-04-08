#include <inttypes.h>
#include <string.h>

#include <X11/extensions/Xrandr.h>

#include "configuration/configuration.h"
#include "event.h" // randr_event_base
#include "frame.h"
#include "frame_sizing.h"
#include "frame_stashing.h"
#include "log.h"
#include "monitor.h"
#include "utility.h"
#include "window.h"
#include "x11_management.h"
#include "xalloc.h"

/* if Xrandr is enabled for usage */
static bool randr_enabled = false;

/* if the Xrandr version supports primary outputs */
static bool randr_has_primary_outputs = false;

/* the first monitor in the monitor linked list */
Monitor *Monitor_first;

/* Create a screenless monitor. */
static Monitor *create_monitor(const char *name, uint32_t name_len)
{
    Monitor *monitor;

    monitor = xcalloc(1, sizeof(*monitor));
    monitor->name = xstrndup(name, name_len);
    return monitor;
}

/* Try to initialize randr. */
void initialize_monitors(void)
{
    int major, minor;

    /* get the event base for XRandr */
    if (XRRQueryExtension(display, &randr_event_base, &randr_error_base)) {
        XRRQueryVersion(display, &major, &minor);

        /* only version 1.2 and upwards is interesting for us */
        if (major > 1 || (major == 1 && minor >= 2)) {
            if (major > 1 || (major == 1 && minor >= 3)) {
                randr_has_primary_outputs = true;
            }
            randr_enabled = true;
            /* get ScreenChangeNotify events */
            XRRSelectInput(display, DefaultRootWindow(display),
                    RRScreenChangeNotifyMask);
        } else {
            /* nevermind */
            randr_event_base = -1;
        }
    }

    merge_monitors(query_monitors());
}

/* Push all dock windows coming after @window. */
static void push_other_dock_windows(Monitor *monitor, FcWindow *window)
{
    const int gravity = get_window_gravity(window);
    if (gravity == StaticGravity) {
        return;
    }

    for (FcWindow *other = window->next; other != NULL; other = other->next) {
        if (!other->state.is_visible) {
            continue;
        }
        if (other->state.mode != WINDOW_MODE_DOCK) {
            continue;
        }

        Monitor *const other_monitor = get_monitor_from_rectangle(
                other->x, other->y, other->width, other->height);
        if (other_monitor != monitor) {
            continue;
        }

        const int other_gravity = get_window_gravity(other);
        switch (gravity) {
        case NorthGravity:
            if (other_gravity == SouthGravity) {
                break;
            }
            if (other_gravity != gravity && other->height > window->height) {
                other->height -= window->height;
            }
            other->y = window->y + window->height;
            break;

        case WestGravity:
            if (other_gravity == EastGravity) {
                break;
            }
            if (other_gravity != gravity && other->width > window->width) {
                other->width -= window->width;
            }
            other->x = window->x + window->width;
            break;

        case SouthGravity:
            if (other_gravity == NorthGravity) {
                break;
            }
            if (other_gravity == gravity) {
                other->y = window->y - other->height;
            } else if (other->height > window->height) {
                other->height -= window->height;
            }
            break;

        case EastGravity:
            if (other_gravity == WestGravity) {
                break;
            }
            if (other_gravity == gravity) {
                other->x = window->x - other->width;
            } else if (other->width > window->width) {
                other->width -= window->width;
            }
            break;

        default:
            break;
        }
    }
}

/* Go through all windows to find the total strut and apply it to all monitors.
 */
void reconfigure_monitor_frames(void)
{
    Monitor *monitor;

    /* reset all struts before recomputing */
    for (monitor = Monitor_first; monitor != NULL; monitor = monitor->next) {
        monitor->strut.left = 0;
        monitor->strut.top = 0;
        monitor->strut.right = 0;
        monitor->strut.bottom = 0;
    }

    /* reconfigure all dock windows */
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
        if (window->state.mode != WINDOW_MODE_DOCK) {
            continue;
        }
        configure_dock_size(window);
    }

    /* recompute all struts and push dock windows */
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
        if (!window->state.is_visible) {
            continue;
        }
        if (window->state.mode != WINDOW_MODE_DOCK) {
            continue;
        }

        monitor = get_monitor_from_rectangle(window->x, window->y,
                window->width, window->height);

        /* check if the strut is applicable to any monitor */
        if (monitor == NULL) {
            continue;
        }

        monitor->strut.left += window->strut.left;
        monitor->strut.top += window->strut.top;
        monitor->strut.right += window->strut.right;
        monitor->strut.bottom += window->strut.bottom;

        push_other_dock_windows(monitor, window);
    }

    /* resize all frames to their according size */
    for (monitor = Monitor_first; monitor != NULL; monitor = monitor->next) {
        const int32_t strut_x = MIN((uint32_t) monitor->strut.left,
                monitor->width);
        const int32_t strut_y = MIN((uint32_t) monitor->strut.top,
                monitor->height);
        const uint32_t strut_sum_x = strut_x + monitor->strut.right;
        const uint32_t strut_sum_y = strut_y + monitor->strut.bottom;
        resize_frame_and_ignore_ratio(monitor->frame,
                monitor->x + strut_x,
                monitor->y + strut_y,
                /* the root frame must be at least 1x1 large */
                strut_sum_x >= monitor->width ? 1 :
                    monitor->width - strut_sum_x,
                strut_sum_y >= monitor->height ? 1 :
                    monitor->height - strut_sum_y);
    }
}

/* Get the overlaping size between the two given rectangles.
 *
 * @return true if the rectangles intersect.
 */
static inline bool get_overlap(int32_t x1, int32_t y1, uint32_t width1,
        uint32_t height1, int32_t x2, int32_t y2, uint32_t width2,
        uint32_t height2, Size *overlap)
{
    int32_t x, y;

    x = MIN(x1 + width1, x2 + width2);
    x -= MAX(x1, x2);

    y = MIN(y1 + height1, y2 + height2);
    y -= MAX(y1, y2);

    if (x > 0 && y > 0) {
        overlap->width = x;
        overlap->height = y;
        return true;
    }
    return false;
}

/* The most efficient way to get the monitor containing given frame. */
Monitor *get_monitor_containing_frame(Frame *frame)
{
    frame = get_root_frame(frame);
    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        if (monitor->frame == frame) {
            return monitor;
        }
    }
    return NULL;
}

/* Get the monitor that overlaps given rectangle the most. */
Monitor *get_monitor_from_rectangle(int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Monitor *best_monitor = NULL;
    uint64_t best_area = 0, area;
    Size overlap;

    /* first check if the center of the rectangle is within any monitor; this
     * might not get the monitor the rectangle overlaps most but this is
     * preferred
     */
    const int32_t center_x = x + width / 2;
    const int32_t center_y = y + height / 2;
    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        const int32_t relative_x = center_x - monitor->x;
        const int32_t relative_y = center_y - monitor->y;
        if (relative_x >= 0 && relative_y >= 0 &&
                relative_x < (int32_t) monitor->width &&
                relative_y < (int32_t) monitor->height) {
            return monitor;
        }
    }

    /* second get the monitor the rectangle overlaps most with */
    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        if (!get_overlap(x, y, width, height, monitor->x, monitor->y,
                    monitor->width, monitor->height, &overlap)) {
            continue;
        }

        area = (uint64_t) overlap.width * overlap.height;
        if (area > best_area) {
            best_monitor = monitor;
            best_area = area;
        }
    }
    return best_monitor;
}

/* Get the monitor that overlaps given rectangle the most or primary if there
 * are there are no intersections.
 */
Monitor *get_monitor_from_rectangle_or_primary(int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Monitor *monitor;

    monitor = get_monitor_from_rectangle(x, y, width, height);
    return monitor == NULL ? Monitor_first : monitor;
}

/* Get a monitor with given name from the monitor linked list. */
static Monitor *get_monitor_by_name(Monitor *monitor,
        const char *name, int name_len)
{
    for (; monitor != NULL; monitor = monitor->next) {
        if (strncmp(monitor->name, name, name_len) == 0 &&
                monitor->name[name_len] == '\0') {
            return monitor;
        }
    }
    return NULL;
}

/* Get a window covering given monitor. */
FcWindow *get_window_covering_monitor(Monitor *monitor)
{
    uint64_t monitor_area;
    FcWindow *best_window = NULL;
    uint64_t best_area = 0, area;
    Size overlap;

    monitor_area = (uint64_t) monitor->width * monitor->height;
    /* go through the windows from bottom to top */
    for (FcWindow *window = Window_bottom;
            window != NULL;
            window = window->above) {
        /* ignore invisible windows */
        if (!window->state.is_visible) {
            continue;
        }
        /* only consider floating and fullscreen windows */
        if (window->state.mode != WINDOW_MODE_FLOATING &&
                window->state.mode != WINDOW_MODE_FULLSCREEN) {
            continue;
        }

        if (!get_overlap(window->x, window->y, window->width, window->height,
                    monitor->x, monitor->y, monitor->width, monitor->height,
                    &overlap)) {
            continue;
        }
        area = (uint64_t) overlap.width * overlap.height;
        /* check if the window covers at least the configured overlap percentage
         * of the monitors area
         */
        if (area * 100 / monitor_area >=
                (uint32_t) configuration.general.overlap_percentage &&
                area >= best_area) {
            best_window = window;
            best_area = area;
        }
    }
    return best_window;
}

/* Get the monitor left of @monitor.
 *
 * The main design goal of this and the three functions below was to have a way
 * to able to access ALL monitors in the most natural way.
 */
Monitor *get_left_monitor(Monitor *monitor)
{
    Monitor *best_monitor = NULL;
    int32_t best_y = INT32_MAX, y;
    int32_t best_right = INT32_MAX;
    bool best_is_y_axis_overlapping = false;

    const int32_t center_y = monitor->y + monitor->height / 2;
    const int32_t right = monitor->x + monitor->width;
    const int32_t bottom = monitor->y + monitor->height;
    for (Monitor *other = Monitor_first; other != NULL; other = other->next) {
        /* ignore monitors not on the left */
        const int32_t other_right = other->x + other->width;
        if (other_right >= right) {
            continue;
        }

        const int32_t other_bottom = other->y + other->height;
        const bool is_y_axis_overlapping =
            other->y < bottom && monitor->y < other_bottom;

        /* ignore any monitors that do not overlap the axis when we already have
         * one that overlaps it
         */
        if (!is_y_axis_overlapping && best_is_y_axis_overlapping) {
            continue;
        }

        /* get the distance from the center */
        if (other->y >= center_y) {
            y = other->y - center_y;
        } else if (other_bottom <= center_y) {
            y = center_y - other_bottom + 1;
        } else {
            y = 0;
        }

        if (best_monitor == NULL ||
                /* if the y axis of this monitor overlaps whereas the previous
                 * does not, we take the monitor as best monitor
                 */
                best_is_y_axis_overlapping != is_y_axis_overlapping ||
                /* if the y axis overlaps, prefer monitors more on the right
                 * over monitors closer to the center
                 */
                (is_y_axis_overlapping &&
                    ((other_right == best_right && y < best_y) ||
                        other_right > best_right)) ||
                /* this preference is flipped if it does not overlap */
                (!is_y_axis_overlapping &&
                    ((y == best_y && other_right > best_right) ||
                        y < best_y))) {
            best_monitor = other;
            best_y = y;
            best_right = other_right;
            best_is_y_axis_overlapping = is_y_axis_overlapping;
        }
    }
    return best_monitor;
}

/* Get the monitor above @monitor. */
Monitor *get_above_monitor(Monitor *monitor)
{
    Monitor *best_monitor = NULL;
    int32_t best_x = INT32_MAX, x;
    int32_t best_bottom = INT32_MAX;
    bool best_is_x_axis_overlapping = false;

    const int32_t center_x = monitor->x + monitor->width / 2;
    const int32_t right = monitor->x + monitor->width;
    const int32_t bottom = monitor->y + monitor->height;
    for (Monitor *other = Monitor_first; other != NULL; other = other->next) {
        /* ignore monitors not below */
        const int32_t other_bottom = other->y + other->height;
        if (other_bottom >= bottom) {
            continue;
        }

        const int32_t other_right = other->x + other->width;
        const bool is_x_axis_overlapping =
            other->x < right && monitor->x < other_right;

        /* ignore any monitors that do not overlap the axis when we already have
         * one that overlaps it
         */
        if (!is_x_axis_overlapping && best_is_x_axis_overlapping) {
            continue;
        }

        /* get the distance from the center */
        if (other->x >= center_x) {
            x = other->x - center_x;
        } else if (other_right <= center_x) {
            x = center_x - other_right + 1;
        } else {
            x = 0;
        }

        if (best_monitor == NULL ||
                /* if the y axis of this monitor overlaps whereas the previous
                 * does not, we take the monitor as best monitor
                 */
                best_is_x_axis_overlapping != is_x_axis_overlapping ||
                /* if the y axis overlaps, prefer monitors more on the right
                 * over monitors closer to the center
                 */
                (is_x_axis_overlapping &&
                    ((other_bottom == best_bottom && x < best_x) ||
                        other_bottom > best_bottom)) ||
                /* this preference is flipped if it does not overlap */
                (!is_x_axis_overlapping &&
                    ((x == best_x && other_bottom > best_bottom) ||
                        x < best_x))) {
            best_monitor = other;
            best_x = x;
            best_bottom = other_bottom;
            best_is_x_axis_overlapping = is_x_axis_overlapping;
        }
    }
    return best_monitor;
}

/* Get the monitor right of @monitor. */
Monitor *get_right_monitor(Monitor *monitor)
{
    Monitor *best_monitor = NULL;
    int32_t best_y = INT32_MAX, y;
    bool best_is_y_axis_overlapping = false;

    const int32_t center_y = monitor->y + monitor->height / 2;
    const int32_t bottom = monitor->y + monitor->height;
    for (Monitor *other = Monitor_first; other != NULL; other = other->next) {
        /* ignore monitors not on the right */
        if (other->x <= monitor->x) {
            continue;
        }

        const int32_t other_bottom = other->y + other->height;
        const bool is_y_axis_overlapping =
            other->y < bottom && monitor->y < other_bottom;

        /* ignore any monitors that do not overlap the axis when we already have
         * one that overlaps it
         */
        if (!is_y_axis_overlapping && best_is_y_axis_overlapping) {
            continue;
        }

        /* get the distance from the center */
        if (other->y >= center_y) {
            y = other->y - center_y;
        } else if (other_bottom <= center_y) {
            y = center_y - other_bottom + 1;
        } else {
            y = 0;
        }

        if (best_monitor == NULL ||
                /* if the y axis of this monitor overlaps whereas the previous
                 * does not, we take the monitor as best monitor
                 */
                best_is_y_axis_overlapping != is_y_axis_overlapping ||
                /* if the y axis overlaps, prefer monitors more on the left
                 * over monitors closer to the center
                 */
                (is_y_axis_overlapping &&
                    ((other->x == best_monitor->x && y < best_y) ||
                        other->x < best_monitor->x)) ||
                /* this preference is flipped if it does not overlap */
                (!is_y_axis_overlapping &&
                    ((y == best_y && other->x < best_monitor->x) ||
                        y < best_y))) {
            best_monitor = other;
            best_y = y;
            best_is_y_axis_overlapping = is_y_axis_overlapping;
        }
    }
    return best_monitor;
}

/* Get the monitor below @monitor. */
Monitor *get_below_monitor(Monitor *monitor)
{
    Monitor *best_monitor = NULL;
    int32_t best_x = INT32_MAX, x;
    bool best_is_x_axis_overlapping = false;

    const int32_t center_x = monitor->x + monitor->width / 2;
    const int32_t right = monitor->x + monitor->width;
    for (Monitor *other = Monitor_first; other != NULL; other = other->next) {
        /* ignore monitors not below */
        if (other->y <= monitor->y) {
            continue;
        }

        const int32_t other_right = other->x + other->width;
        const bool is_x_axis_overlapping =
            other->x < right && monitor->x < other_right;

        /* ignore any monitors that do not overlap the axis when we already have
         * one that overlaps it
         */
        if (!is_x_axis_overlapping && best_is_x_axis_overlapping) {
            continue;
        }

        /* get the distance from the center */
        if (other->x >= center_x) {
            x = other->x - center_x;
        } else if (other_right <= center_x) {
            x = center_x - other_right + 1;
        } else {
            x = 0;
        }

        if (best_monitor == NULL ||
                /* if the y axis of this monitor overlaps whereas the previous
                 * does not, we take the monitor as best monitor
                 */
                best_is_x_axis_overlapping != is_x_axis_overlapping ||
                /* if the y axis overlaps, prefer monitors more on the right
                 * over monitors closer to the center
                 */
                (is_x_axis_overlapping &&
                    ((other->y == best_monitor->y && x < best_x) ||
                        other->y < best_monitor->y)) ||
                /* this preference is flipped if it does not overlap */
                (!is_x_axis_overlapping &&
                    ((x == best_x && other->y < best_monitor->y) ||
                        x < best_x))) {
            best_monitor = other;
            best_x = x;
            best_is_x_axis_overlapping = is_x_axis_overlapping;
        }
    }
    return best_monitor;
}

/* Get the first monitor matching given pattern. */
Monitor *get_monitor_by_pattern(const char *pattern)
{
    for (Monitor *monitor = Monitor_first;
            monitor != NULL;
            monitor = monitor->next) {
        if (matches_pattern(pattern, monitor->name)) {
            return monitor;
        }
    }
    return NULL;
}

/* Get the primary randr output or `None` if there is none. */
RROutput get_primary_output(void)
{
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
    if (randr_has_primary_outputs) {
        return XRRGetOutputPrimary(display, DefaultRootWindow(display));
    }
#endif
    return None;
}

/* Get a list of monitors that are associated to the screen. */
Monitor *query_monitors(void)
{
    RROutput primary_output;

    XRRScreenResources *resources;

    Monitor *monitor;
    Monitor *first_monitor = NULL, *last_monitor, *primary_monitor = NULL;

    if (!randr_enabled) {
        return NULL;
    }

    primary_output = get_primary_output();

    resources = XRRGetScreenResources(display, DefaultRootWindow(display));

    /* go through the outputs */
    for (int i = 0; i < resources->noutput; i++) {
        RROutput output;
        XRROutputInfo *info;
        XRRCrtcInfo *crtc;

        output = resources->outputs[i];

        /* get the output information which includes the output name */
        info = XRRGetOutputInfo(display, resources, output);

        /* only continue with outputs that have a size */
        if (info->crtc == None) {
            continue;
        }

        /* get the crtc (cathodic ray tube configuration, basically an old TV)
         * which tells us the size of the output
         */
        crtc = XRRGetCrtcInfo(display, resources, info->crtc);

        LOG("output %.*s: %R\n", info->nameLen, info->name,
                crtc->x, crtc->y, crtc->width, crtc->height);

        monitor = create_monitor(info->name, info->nameLen);

        /* add the monitor to the linked list */
        if (first_monitor == NULL) {
            first_monitor = monitor;
            last_monitor = first_monitor;
        } else {
            if (primary_output == output) {
                primary_monitor = last_monitor;
            } else {
                last_monitor->next = monitor;
                last_monitor = last_monitor->next;
            }
        }

        monitor->x = crtc->x;
        monitor->y = crtc->y;
        monitor->width = crtc->width;
        monitor->height = crtc->height;

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(info);
    }

    /* add the primary monitor to the start of the list */
    if (primary_monitor != NULL) {
        primary_monitor->next = first_monitor;
        first_monitor = primary_monitor;
    }

    XRRFreeScreenResources(resources);

    /* remove monitors that are contained within other monitors */
    for (Monitor *monitor = first_monitor;
            monitor != NULL;
            monitor = monitor->next) {
        /* go through all monitors coming after */
        for (Monitor *previous, *next = monitor;; ) {
            previous = next;
            next = next->next;
            if (next == NULL) {
                break;
            }

            const int32_t right = monitor->x + monitor->width;
            const int32_t bottom = monitor->y + monitor->height;
            const int32_t next_right = next->x + next->width;
            const int32_t next_bottom = next->y + next->height;
            /* check if `monitor` is within `next` */
            if (monitor->x >= next->x && monitor->y >= next->y &&
                    right <= next_right && bottom <= next_bottom) {
                /* inflate the monitor */
                monitor->x = next->x;
                monitor->y = next->y;
                monitor->width = next->width;
                monitor->height = next->height;
            /* check if `next` is within `monitor` */
            } else if (next->x >= monitor->x && next->y >= monitor->y &&
                    next_right <= right && next_bottom <= bottom) {
                /* nothing */
            } else {
                /* they are not contained in any way */
                continue;
            }

            LOG("merged monitor %s into %s\n", next->name, monitor->name);

            previous->next = next->next;
            free(next->name);
            free(next);

            next = previous;
        }
    }

    return first_monitor;
}

/* Merge given monitor linked list into the screen.
 *
 * The current rule is to keep monitors from the source and delete monitors no
 * longer in the list.
 */
void merge_monitors(Monitor *monitors)
{
    Frame *focus_frame_root;

    if (monitors == NULL) {
        monitors = create_monitor("default", UINT32_MAX);
        monitors->width = DisplayWidth(display, DefaultScreen(display));
        monitors->height = DisplayHeight(display, DefaultScreen(display));
    }

    /* copy frames from the old monitors to the new ones with same name */
    for (Monitor *monitor = monitors; monitor != NULL;
            monitor = monitor->next) {
        Monitor *const other = get_monitor_by_name(Monitor_first,
                monitor->name, strlen(monitor->name));
        if (other == NULL) {
            continue;
        }

        monitor->frame = other->frame;
        other->frame = NULL;
    }

    focus_frame_root = get_root_frame(Frame_focus);
    /* drop the frames that are no longer valid */
    for (Monitor *monitor = Monitor_first, *next_monitor; monitor != NULL;
            monitor = next_monitor) {
        next_monitor = monitor->next;
        if (monitor->frame != NULL) {
            /* make sure no broken frame focus remains */
            if (focus_frame_root == monitor->frame) {
                Frame_focus = NULL;
            }

            /* stash away the frame */
            stash_frame(monitor->frame);
            destroy_frame(monitor->frame);
        }
        free(monitor->name);
        free(monitor);
    }

    Monitor_first = monitors;

    /* initialize the remaining monitors' frames */
    for (Monitor *monitor = monitors; monitor != NULL;
            monitor = monitor->next) {
        if (monitor->frame == NULL) {
            if (configuration.tiling.auto_fill_void) {
                monitor->frame = pop_stashed_frame();
            }
            /* the popped frame might also be NULL which is why this is NOT in
             * an else statement
             */
            if (monitor->frame == NULL) {
                monitor->frame = create_frame();
            }
            /* set the initial size */
            monitor->frame->x = monitor->x;
            monitor->frame->y = monitor->y;
            monitor->frame->width = monitor->width;
            monitor->frame->height = monitor->height;
        }
    }

    /* if the focus frame was abonded, focus a different one */
    if (Frame_focus == NULL) {
        set_focus_frame(Monitor_first->frame);
    }
}
