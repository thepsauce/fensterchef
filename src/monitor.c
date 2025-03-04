#include <inttypes.h>
#include <string.h>

#include <xcb/xcb_renderutil.h>

#include "configuration.h"
#include "event.h" // randr_event_base
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "x11_management.h"
#include "xalloc.h"

/* if randr is enabled for usage */
static bool randr_enabled = false;

/* the first monitor in the monitor linked list */
Monitor *first_monitor;

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
    const xcb_query_extension_reply_t *extension;
    xcb_generic_error_t *error;
    xcb_randr_query_version_cookie_t version_cookie;
    xcb_randr_query_version_reply_t *version;

    /* get the data for the randr extension and check if it is there */
    extension = xcb_get_extension_data(connection, &xcb_randr_id);
    if (!extension->present) {
        return;
    }

    /* get the randr version number, currently not used for anything */
    version_cookie = xcb_randr_query_version(connection,
            XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION);
    version = xcb_randr_query_version_reply(connection, version_cookie, &error);
    if (error != NULL) {
        LOG_ERROR("could not query randr version: %E\n", error);
        free(error);
    } else {
        free(version);

        randr_enabled = true;
        randr_event_base = extension->first_event;

        xcb_randr_select_input(connection, screen->root,
                XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);
    }

    merge_monitors(query_monitors());
}

/* Get the monitor that overlaps given rectangle the most. */
Monitor *get_monitor_from_rectangle(int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Monitor *best_monitor = NULL;
    uint64_t best_area = 0, area;
    Point overlap;

    for (Monitor *monitor = first_monitor; monitor != NULL;
            monitor = monitor->next) {
        overlap.x = MIN(x + width, monitor->x + monitor->width);
        overlap.x -= MAX(x, monitor->x);

        overlap.y = MIN(y + height, monitor->y + monitor->height);
        overlap.y -= MAX(y, monitor->y);

        if (overlap.x <= 0 || overlap.y <= 0) {
            continue;
        }

        area = overlap.x * overlap.y;
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
    return monitor == NULL ? first_monitor : monitor;
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

/* Gets a list of monitors that are associated to the screen. */
Monitor *query_monitors(void)
{
    xcb_generic_error_t *error;

    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary;
    xcb_randr_output_t primary_output;

    xcb_randr_get_screen_resources_current_cookie_t resources_cookie;
    xcb_randr_get_screen_resources_current_reply_t *resources;

    xcb_randr_output_t *outputs;
    int output_count;

    Monitor *monitor;
    Monitor *first_monitor = NULL, *last_monitor, *primary_monitor = NULL;

    xcb_randr_get_output_info_cookie_t output_cookie;
    xcb_randr_get_output_info_reply_t *output;

    char *name;
    int name_length;

    xcb_randr_get_crtc_info_cookie_t crtc_cookie;
    xcb_randr_get_crtc_info_reply_t *crtc;

    if (!randr_enabled) {
        return NULL;
    }

    /* get cookies for later */
    primary_cookie = xcb_randr_get_output_primary(connection,
            screen->root);
    resources_cookie = xcb_randr_get_screen_resources_current(connection,
            screen->root);

    /* get the primary monitor */
    primary = xcb_randr_get_output_primary_reply(connection, primary_cookie,
            NULL);
    if (primary == NULL) {
        primary_output = XCB_NONE;
    } else {
        primary_output = primary->output;
        free(primary);
    }

    /* get the screen resources for querying the screen outputs */
    resources = xcb_randr_get_screen_resources_current_reply(connection,
            resources_cookie, &error);
    if (error != NULL) {
        LOG_ERROR("could not get screen resources: %E\n", error);
        free(error);
        return NULL;
    }

    /* get the outputs (monitors) */
    outputs = xcb_randr_get_screen_resources_current_outputs(resources);
    output_count =
        xcb_randr_get_screen_resources_current_outputs_length(resources);

    for (int i = 0; i < output_count; i++) {
        /* get the output information that includes the output name */
        output_cookie = xcb_randr_get_output_info(connection, outputs[i],
               resources->timestamp);
        output = xcb_randr_get_output_info_reply(connection, output_cookie,
                &error);
        if (error != NULL) {
            LOG_ERROR("unable to get output info of %d: %E\n", i, error);
            free(error);
            continue;
        }

        /* extract the name information from the reply */
        name = (char*) xcb_randr_get_output_info_name(output);
        name_length = xcb_randr_get_output_info_name_length(output);

        if (output->crtc == XCB_NONE) {
            LOG("ignored output %.*s: no crtc\n", name_length, name);
            continue;
        }

        /* get the crtc (cathodic ray tube configuration, basically an old TV)
         * which tells us the size of the output
         */
        crtc_cookie = xcb_randr_get_crtc_info(connection, output->crtc,
                resources->timestamp);

        crtc = xcb_randr_get_crtc_info_reply(connection, crtc_cookie, &error);
        if (crtc == NULL) {
            LOG_ERROR("output %.*s gave a NULL crtc: %E\n", name_length, name,
                    error);
            free(error);
            continue;
        }

        LOG("output %.*s: %R\n", name_length, name,
                crtc->x, crtc->y, crtc->width, crtc->height);

        monitor = create_monitor(name, name_length);

        /* add the monitor to the linked list */
        if (first_monitor == NULL) {
            first_monitor = monitor;
            last_monitor = first_monitor;
        } else {
            if (primary_output == outputs[i]) {
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

        free(crtc);
    }

    /* add the primary monitor to the start of the list */
    if (primary_monitor != NULL) {
        primary_monitor->next = first_monitor;
        first_monitor = primary_monitor;
    }

    return first_monitor;
}

/* Merge given monitor linked list into the screen.
 *
 * The main purpose of this function is to essentially make the linked in screen
 * be @monitors, but it is not enough to say: `first_monitor = monitors`.
 *
 * The current rule is to keep monitors from the source and delete monitors no
 * longer in the list.
 */
void merge_monitors(Monitor *monitors)
{
    if (monitors == NULL) {
        monitors = create_monitor("default", UINT32_MAX);
        monitors->width = screen->width_in_pixels;
        monitors->height = screen->height_in_pixels;
    }

    /* copy frames from the old monitors to the new ones with same name */
    for (Monitor *monitor = monitors; monitor != NULL;
            monitor = monitor->next) {
        Monitor *const other = get_monitor_by_name(first_monitor,
                monitor->name, strlen(monitor->name));
        if (other == NULL) {
            continue;
        }

        monitor->frame = other->frame;
        other->frame = NULL;
    }

    /* drop the frames that are no longer valid or add them again */
    for (Monitor *monitor = first_monitor, *next_monitor; monitor != NULL;
            monitor = next_monitor) {
        next_monitor = monitor->next;
        if (monitor->frame != NULL) {
            /* make sure no broken frame focus remains */
            if (get_root_frame(focus_frame) == monitor->frame) {
                focus_frame = NULL;
            }

            /* stash away the frame */
            stash_frame(monitor->frame);
            free(monitor->frame);
        }
        free(monitor->name);
        free(monitor);
    }

    first_monitor = monitors;

    /* initialize the remaining monitors' frames */
    for (Monitor *monitor = monitors; monitor != NULL;
            monitor = monitor->next) {
        if (monitor->frame == NULL) {
            if (configuration.tiling.auto_fill_void) {
                monitor->frame = pop_stashed_frame();
            } else {
                monitor->frame = xcalloc(1, sizeof(*monitor->frame));
            }
        }
    }

    /* if the focus frame was abonded, focus a different one */
    if (focus_frame == NULL) {
        set_focus_frame(first_monitor->frame);
    }
}
