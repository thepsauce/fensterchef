#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include "configuration.h"
#include "event.h"
#include "fensterchef.h"
#include "frame.h"
#include "keymap.h"
#include "log.h"
#include "monitor.h"
#include "render.h"
#include "stash_frame.h"
#include "utility.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"

/* user window list window */
struct window_list window_list;

/* Create the window list. */
int initialize_window_list(void)
{
    xcb_generic_error_t *error;
    const char *window_list_name = "[fensterchef] window list";

    window_list.client.id = xcb_generate_id(connection);
    /* indicate to not manage the window */
    general_values[0] = true;
    /* get key press events, focus change events and expose events */
    general_values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_FOCUS_CHANGE;
    error = xcb_request_check(connection, xcb_create_window_checked(connection,
                XCB_COPY_FROM_PARENT, window_list.client.id,
                screen->root, -1, -1, 1, 1, 0,
                XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, general_values));
    if (error != NULL) {
        LOG_ERROR("could not create window list window: %E\n", error);
        free(error);
        return ERROR;
    }
    window_list.client.x = -1;
    window_list.client.y = -1;
    window_list.client.width = 1;
    window_list.client.height = 1;
    xcb_icccm_set_wm_name(connection, window_list.client.id,
            ATOM(UTF8_STRING), 8, strlen(window_list_name), window_list_name);
    return OK;
}

/* Check if @window should appear in the window list. */
static bool is_valid_for_display(Window *window)
{
    return does_window_accept_focus(window);
}

/* Get character indicating the window state. */
static inline char get_indicator_character(Window *window)
{
    return !window->state.is_visible ? '-' :
            window->state.mode == WINDOW_MODE_FLOATING ?
                (window == focus_window ? '#' : '=') :
            window->state.mode == WINDOW_MODE_FULLSCREEN ?
                (window == focus_window ? '@' : 'F') :
            window == focus_window ? '*' : '+';
}

static inline void get_window_string(Window *window, utf8_t *buffer,
        size_t buffer_size)
{
    snprintf((char*) buffer, buffer_size, "%" PRIu32 "%c%s",
            window->number, get_indicator_character(window), window->name);
}

/* Render the window list. */
static void render_window_list(void)
{
    utf8_t                  buffer[256];
    uint32_t                window_count;
    struct text_measure     measure;
    uint32_t                height_per_item;
    uint32_t                max_width;
    Frame                   *root_frame;
    uint32_t                index = 0;
    uint32_t                maximum_item;
    xcb_rectangle_t         rectangle;
    xcb_render_color_t      background_color;
    xcb_render_picture_t    pen;

    /* measure the maximum needed width and get the index of the currently
     * selected window
     */
    window_count = 0;
    measure.ascent = 12;
    measure.descent = -4;
    max_width = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        if (window_list.selected == window) {
            index = window_count;
        }

        window_count++;
        get_window_string(window, buffer, sizeof(buffer));
        measure_text(buffer, strlen((char*) buffer), &measure);
        max_width = MAX(max_width, measure.total_width);
    }

    /* unmap the window list if there are no more windows */
    if (window_count == 0) {
        unmap_client(&window_list.client);
        return;
    }

    height_per_item = measure.ascent - measure.descent +
        configuration.notification.padding;

    root_frame = get_root_frame(focus_frame);

    /* the number of items that can fit on screen */
    maximum_item = (root_frame->height -
            configuration.notification.border_size) / height_per_item;
    maximum_item = MIN(maximum_item, window_count);

    /* adjust the scrolling so the selected item is visible */
    if (index < window_list.vertical_scrolling) {
        window_list.vertical_scrolling = index;
    }
    if (index >= window_list.vertical_scrolling + maximum_item) {
        window_list.vertical_scrolling = index - maximum_item + 1;
    }

    /* set the list position and size so it is in the top right of the monitor
     * containing the focus frame
     */
    configure_client(&window_list.client,
            root_frame->x + root_frame->width - max_width -
                configuration.notification.padding / 2 -
                configuration.notification.border_size * 2,
            root_frame->y,
            max_width + configuration.notification.padding / 2,
            maximum_item * height_per_item,
            window_list.client.border_width);

    /* render the items showing the window names */
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = max_width + configuration.notification.padding / 2;
    rectangle.height = height_per_item;
    index = 0;
    for (Window *window = first_window; window != NULL; window = window->next) {
        if (!is_valid_for_display(window)) {
            continue;
        }

        index++;
        /* if the item is above the scrolling, ignore it */
        if (index <= window_list.vertical_scrolling) {
            continue;
        }
        /* if the item is below the visible region, stop */
        if (index > window_list.vertical_scrolling + maximum_item) {
            break;
        }

        /* use normal or inverted colors */
        if (window != window_list.selected) {
            pen = stock_objects[STOCK_BLACK_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.background);
        } else {
            pen = stock_objects[STOCK_WHITE_PEN];
            convert_color_to_xcb_color(&background_color,
                    configuration.notification.foreground);
        }

        /* draw the text centered within the item */
        get_window_string(window, buffer, sizeof(buffer));
        draw_text(window_list.client.id, buffer,
                strlen((char*) buffer), background_color, &rectangle,
                pen, configuration.notification.padding / 2,
                rectangle.y + measure.ascent +
                    configuration.notification.padding / 2);

        rectangle.y += rectangle.height;
    }
}

/* Get the window before @start in the window list. @last_valid is the fallback
 * value and @before is the end point.
 */
static Window *get_valid_window_before(Window *last_valid, Window *start,
        Window *before)
{
    while (start != before) {
        if (is_valid_for_display(start)) {
            last_valid = start;
        }
        start = start->next;
    }
    return last_valid;
}

/* Get the window after @start. @last_valid is the fallback value. This does not
 * have a third parameter because it would just be NULL anyway for all calls.
 */
static Window *get_valid_window_after(Window *last_valid, Window *start)
{
    while (start != NULL) {
        if (is_valid_for_display(start)) {
            last_valid = start;
            break;
        }
        start = start->next;
    }
    return last_valid;
}

/* Show @window and focus it.
 *
 * @shift controls how the window appears. Either it is put directly into the
 * current frame or simply shown which allows auto splitting to occur.
 */
static inline void focus_and_let_window_appear(Window *window, bool shift)
{
    /* if shift is down, show the window in the current frame */
    if (shift && !window->state.is_visible) {
        /* clear the old frame and stash it */
        (void) stash_frame(focus_frame);
        /* put the window into the focused frame, size and show it */
        set_window_mode(window, WINDOW_MODE_TILING);
        focus_frame->window = window;
        reload_frame(focus_frame);
        window->state.is_visible = true;
    } else {
        /* put floating windows on the top */
        update_window_layer(window);

        show_window(window);
    }
    set_focus_window_with_frame(window);
}

/* Handle a key press for the window list window. */
static void handle_key_press(xcb_key_press_event_t *event)
{
    if (event->event != window_list.client.id) {
        return;
    }

    switch (get_keysym(event->detail)) {
    /* cancel selection */
    case XK_q:
    case XK_n:
    case XK_Escape:
        unmap_client(&window_list.client);
        break;

    /* confirm selection */
    case XK_y:
    case XK_Return:
        if (window_list.selected != NULL &&
                window_list.selected != focus_window) {
            focus_and_let_window_appear(window_list.selected,
                    !!(event->state & XCB_MOD_MASK_SHIFT));
            window_list.should_revert_focus = false;
        }
        unmap_client(&window_list.client);
        break;

    /* go to the first item */
    case XK_Home:
        window_list.selected = get_valid_window_after(NULL, first_window);
        break;

    /* go to the last item */
    case XK_End:
        window_list.selected = get_valid_window_before(window_list.selected,
                first_window, NULL);
        break;

    /* go to the previous item */
    case XK_h:
    case XK_k:
    case XK_Left:
    case XK_Up:
        window_list.selected = get_valid_window_before(window_list.selected,
                first_window, window_list.selected);
        break;

    /* go to the next item */
    case XK_l:
    case XK_j:
    case XK_Right:
    case XK_Down:
        window_list.selected = get_valid_window_after(window_list.selected,
                window_list.selected->next);
        break;
    }
}

/* Handle a FocusOut event. */
static void handle_focus_out(xcb_focus_out_event_t *event)
{
    /* if the this event is for the window list and it is mapped (we also get
     * this event after the window was unmapped)
     */
    if (event->event != window_list.client.id ||
            !window_list.client.is_mapped) {
        return;
    }

    /* if someone grabbed the focus, we expect it to return soon */
    if (event->mode == XCB_NOTIFY_MODE_GRAB) {
        return;
    }

    /* refocus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
            window_list.client.id, XCB_CURRENT_TIME);
}

/* Handle an UnmapNotify event. */
static void handle_unmap_notify(xcb_unmap_notify_event_t *event)
{
    if (event->window != window_list.client.id) {
        return;
    }

    /* give focus back to the last window */
    if (window_list.should_revert_focus) {
        set_input_focus(focus_window);
    }
}

/* Handle when a window gets destroyed. */
static void handle_destroy_notify(xcb_destroy_notify_event_t *event)
{
    if (!window_list.client.is_mapped) {
        return;
    }

    Window *const window = get_window_of_xcb_window(
            ((xcb_unmap_notify_event_t*) event)->window);
    /* if the currently selected window is destroyed, select a different one */
    if (window == window_list.selected) {
        window_list.selected = get_valid_window_before(
                get_valid_window_after(NULL, window_list.selected->next),
                first_window, window_list.selected);
    }
}

/* Handle an incoming event for the window list. */
void handle_window_list_event(xcb_generic_event_t *event)
{
    /* remove the most significant bit, this gets the actual event type */
    switch ((event->response_type & ~0x80)) {
    /* a key was pressed */
    case XCB_KEY_PRESS:
        handle_key_press((xcb_key_press_event_t*) event);
        break;

    /* the window list lost focus */
    case XCB_FOCUS_OUT:
        handle_focus_out((xcb_focus_out_event_t*) event);
        break;

    /* the window list was hidden */
    case XCB_UNMAP_NOTIFY:
        handle_unmap_notify((xcb_unmap_notify_event_t*) event);
        break;

    /* select another window if the currently selected one is destroyed */
    case XCB_DESTROY_NOTIFY:
        handle_destroy_notify((xcb_destroy_notify_event_t*) event);
        break;
    }

    if (window_list.client.is_mapped) {
        render_window_list();
    }
}

/* Show the window list on screen. */
int show_window_list(void)
{
    Window *selected;

    /* hide the window list if it is already there */
    if (window_list.client.is_mapped) {
        return ERROR;
    }

    /* get the initially selected window */
    if (focus_window != NULL) {
        selected = focus_window;
    } else {
        for (selected = first_window; selected != NULL;
                selected = selected->next) {
            if (is_valid_for_display(selected)) {
                break;
            }
        }
    }

    if (selected == NULL) {
        set_notification((utf8_t*) "No managed windows",
                focus_frame->x + focus_frame->width / 2,
                focus_frame->y + focus_frame->height / 2);
        return ERROR;
    }

    window_list.selected = selected;
    window_list.should_revert_focus = true;

    /* show the window list window on screen */
    map_client(&window_list.client);

    /* raise the window */
    general_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, window_list.client.id,
            XCB_CONFIG_WINDOW_STACK_MODE, general_values);

    /* focus the window list */
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
            window_list.client.id, XCB_CURRENT_TIME);
    return OK;
}
