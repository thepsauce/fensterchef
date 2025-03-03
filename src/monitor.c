#include <inttypes.h>
#include <string.h>

#include <xcb/xcb_renderutil.h>

#include "event.h" // randr_event_base
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"
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
    version_cookie = xcb_randr_query_version(connection, XCB_RANDR_MAJOR_VERSION,
            XCB_RANDR_MINOR_VERSION);
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

/* Get a monitor marked as primary or the first monitor if no monitor is marked
 * as primary.
 */
Monitor *get_primary_monitor(void)
{
    Monitor *candidate = NULL;

    for (Monitor *monitor = first_monitor; monitor != NULL;
            monitor = monitor->next) {
        if (monitor->is_primary) {
            return monitor;
        }
        candidate = monitor;
    }
    return candidate;
}

/* Get the monitor that overlaps given rectangle the most. */
Monitor *get_monitor_from_rectangle(int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Monitor *best_monitor = NULL;
    uint64_t best_area = 0, area;
    int32_t x_overlap, y_overlap;

    for (Monitor *monitor = first_monitor; monitor != NULL;
            monitor = monitor->next) {
        x_overlap = MIN(x + width,
                monitor->x + monitor->width);
        x_overlap -= MAX(x, monitor->x);

        y_overlap = MIN(y + height,
                monitor->y + monitor->height);
        y_overlap -= MAX(y, monitor->y);

        if (best_monitor == NULL) {
            best_monitor = monitor;
        }

        if (x_overlap <= 0 || y_overlap <= 0) {
            continue;
        }

        area = x_overlap * y_overlap;
        if (area > best_area) {
            best_monitor = monitor;
            best_area = area;
        }
    }
    return best_monitor;
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

    Monitor *first_monitor, *last_monitor;

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

    first_monitor = NULL;

    for (int i = 0; i < output_count; i++) {
        /* get the output information that includes the output name */
        output_cookie = xcb_randr_get_output_info(connection, outputs[i],
               resources->timestamp);
        output = xcb_randr_get_output_info_reply(connection, output_cookie, &error);
        if (error != NULL) {
            LOG_ERROR("unable to get output info of %d: %E\n", i, error);
            free(error);
            continue;
        }

        /* extract the name information from the reply */
        name = (char*) xcb_randr_get_output_info_name(output);
        name_length = xcb_randr_get_output_info_name_length(output);

        if (output->connection != XCB_RANDR_CONNECTION_CONNECTED) {
            LOG("ignored output %.*s: not connected\n", name_length, name);
            continue;
        }

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

        LOG("output %.*s: %R\n", crtc->x, crtc->y, crtc->width, crtc->height);

        /* add the monitor to the linked list */
        if (first_monitor == NULL) {
            first_monitor = create_monitor(name, name_length);
            last_monitor = first_monitor;
        } else {
            last_monitor->next = create_monitor(name, name_length);
            last_monitor = last_monitor->next;
        }

        last_monitor->is_primary = primary_output == outputs[i];
        last_monitor->x = crtc->x;
        last_monitor->y = crtc->y;
        last_monitor->width = crtc->width;
        last_monitor->height = crtc->height;
    }
    return first_monitor;
}

/* Merges given monitor linked list into the screen.
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
        Monitor *const named_monitor = get_monitor_by_name(first_monitor,
                monitor->name, strlen(monitor->name));
        if (named_monitor == NULL) {
            continue;
        }

        monitor->frame = named_monitor->frame;
        named_monitor->frame = NULL;
    }

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

    /* get frames from the stash to fill the monitors or initialize them as
     * empty
     */
    for (Monitor *monitor = monitors; monitor != NULL;
            monitor = monitor->next) {
        if (monitor->frame == NULL) {
            monitor->frame = xcalloc(1, sizeof(*monitor->frame));
            fill_void_with_stash(monitor->frame);
        }
    }

    /* if the focus frame was abonded, focus a different one */
    if (focus_frame == NULL) {
        set_focus_frame(get_primary_monitor()->frame);
    }
}
