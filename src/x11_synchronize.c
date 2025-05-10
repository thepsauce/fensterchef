#include <X11/XKBlib.h>
#include <X11/Xproto.h>

#include "binding.h"
#include "cursor.h"
#include "frame.h"
#include "log.h"
#include "window.h"
#include "window_properties.h"
#include "window_stacking.h"
#include "x11_synchronize.h"

/* hints set by all parts of the program indicating that a specific part needs
 * to be synchronized
 */
unsigned synchronization_flags;

/* first index of an xkb event */
int xkb_event_base, xkb_error_base;

/* first index of a randr event */
int randr_event_base, randr_error_base;

/* display to the X server */
Display *display;

/* Open the X11 display (X server connection). */
void open_connection(void)
{
    int major_version, minor_version;
    int status;

    major_version = XkbMajorVersion;
    minor_version = XkbMinorVersion;
    display = XkbOpenDisplay(NULL, &xkb_event_base, &xkb_error_base,
            &major_version, &minor_version, &status);
    if (display == NULL) {
        LOG_ERROR("could not open display: " COLOR(RED));
        switch (status) {
        case XkbOD_BadLibraryVersion:
            fprintf(stderr, "using a bad XKB library version\n");
            break;

        case XkbOD_ConnectionRefused:
            fprintf(stderr, "could not open connection\n");
            break;

        case XkbOD_BadServerVersion:
            fprintf(stderr, "the server and client XKB versions mismatch\n");
            break;

        case XkbOD_NonXkbServer:
            fprintf(stderr, "the server does not have the XKB extension\n");
            break;
        }
        exit(EXIT_FAILURE);
    }

    /* receive events when the keyboard changes */
    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbNewKeyboardNotify,
            XkbNKN_KeycodesMask,
            XkbNKN_KeycodesMask);
    /* listen for changes to key symbols and the modifier map */
    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbMapNotify,
            XkbAllClientInfoMask,
            XkbAllClientInfoMask);

    x_file_descriptor = XConnectionNumber(display);

    LOG("%D\n", display);
}

/* Set the client list root property. */
static void synchronize_client_list(void)
{
    /* a list of window ids that is synchronized with the actual windows */
    static struct {
        /* the id of the window */
        Window *ids;
        /* the number of allocated ids */
        unsigned length;
    } client_list;

    Window root;
    FcWindow *window;
    unsigned index = 0;

    root = DefaultRootWindow(display);

    /* allocate more ids if needed or trim if there are way too many */
    if (Window_count > client_list.length ||
            MAX(Window_count, 4) * 4 < client_list.length) {
        LOG_DEBUG("resizing client list to %u\n",
                Window_count);
        client_list.length = Window_count;
        REALLOCATE(client_list.ids, client_list.length);
    }

    /* sort the list in order of the Z stacking (bottom to top) */
    for (window = Window_bottom; window != NULL; window = window->above) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST_STACKING` property */
    XChangeProperty(display, root, ATOM(_NET_CLIENT_LIST_STACKING),
            XA_WINDOW, 32, PropModeReplace,
            (unsigned char*) client_list.ids, Window_count);

    index = 0;
    /* sort the list in order of their age (oldest to newest) */
    for (window = Window_oldest; window != NULL; window = window->newer) {
        client_list.ids[index] = window->client.id;
        index++;
    }
    /* set the `_NET_CLIENT_LIST` property */
    XChangeProperty(display, root, ATOM(_NET_CLIENT_LIST),
            XA_WINDOW, 32, PropModeReplace,
            (unsigned char*) client_list.ids, Window_count);
}

/* Recursively check if the window is contained within @frame. */
static bool is_window_part_of(FcWindow *window, Frame *frame)
{
    if (frame->left != NULL) {
        return is_window_part_of(window, frame->left) ||
            is_window_part_of(window, frame->right);
    }
    return frame->window == window;
}

/* Synchronize the local data with the X server. */
void synchronize_with_server(unsigned flags)
{
    Atom atoms[2];
    FcWindow *window;

    if (flags == 0) {
        flags = synchronization_flags;
    }

    if ((flags & SYNCHRONIZE_ROOT_CURSOR)) {
        XDefineCursor(display, DefaultRootWindow(display),
                load_cursor(CURSOR_ROOT, NULL));
        synchronization_flags &= ~SYNCHRONIZE_ROOT_CURSOR;
    }

    if ((flags & SYNCHRONIZE_BUTTON_BINDING)) {
        for (FcWindow *window = Window_first;
                window != NULL;
                window = window->next) {
            grab_configured_buttons(window->client.id);
        }
        synchronization_flags &= ~SYNCHRONIZE_BUTTON_BINDING;
    }

    if ((flags & SYNCHRONIZE_KEY_BINDING)) {
        grab_configured_keys();
        synchronization_flags &= ~SYNCHRONIZE_KEY_BINDING;
    }

    /* since the strut of a monitor might have changed because a window with
     * strut got hidden or shown, we need to recompute those
     */
    reconfigure_monitor_frames();

    /* set the borders of the windows and manage the focused state */
    for (window = Window_first; window != NULL; window = window->next) {
        /* remove the focused state from windows that are not focused */
        if (window != Window_focus) {
            atoms[0] = ATOM(_NET_WM_STATE_FOCUSED);
            remove_window_states(window, atoms, 1);
        }

        /* update the border size */
        if (is_window_borderless(window)) {
            window->border_size = 0;
        } else {
            window->border_size = configuration.border_size;
        }

        /* set the color of the focused window */
        if (window == Window_focus) {
            window->border_color = configuration.border_color_focus;
        /* deeply set the colors of all windows within the focused frame */
        } else if ((Window_focus == NULL ||
                        Window_focus->state.mode == WINDOW_MODE_TILING) &&
                window->state.mode == WINDOW_MODE_TILING &&
                is_window_part_of(window, Frame_focus)) {
            window->border_color = configuration.border_color_focus;
        /* if the window is the top window or within the focused frame, give it
         * the active color
         */
        } else if (is_window_part_of(window, Frame_focus) ||
                (window->state.mode == WINDOW_MODE_FLOATING &&
                    window == Window_top)) {
            window->border_color = configuration.border_color_active;
        } else {
            window->border_color = configuration.border_color;
        }
    }

    synchronize_window_stacking_order();

    /* update the client list and stacking order */
    /* TODO: only change if it actually changed */
    synchronize_client_list();

    /* configure all visible windows and map them */
    for (window = Window_top; window != NULL; window = window->below) {
        if (!window->state.is_visible) {
            continue;
        }
        configure_client(&window->client, window->x, window->y,
                window->width, window->height, window->border_size);
        change_client_attributes(&window->client, window->border_color);

        atoms[0] = ATOM(_NET_WM_STATE_HIDDEN);
        remove_window_states(window, atoms, 1);

        if (window->wm_state != NormalState) {
            window->wm_state = NormalState;
            atoms[0] = NormalState;
            /* no icon */
            atoms[1] = None;
            XChangeProperty(display, window->client.id, ATOM(WM_STATE),
                    ATOM(WM_STATE), 32, PropModeReplace,
                    (unsigned char*) atoms, 2);
        }

        map_client(&window->client);
    }

    /* unmap all invisible windows */
    for (FcWindow *window = Window_bottom;
            window != NULL;
            window = window->above) {
        if (window->state.is_visible) {
            continue;
        }

        atoms[0] = ATOM(_NET_WM_STATE_HIDDEN);
        add_window_states(window, atoms, 1);

        if (window->wm_state != WithdrawnState) {
            window->wm_state = WithdrawnState;
            atoms[0] = WithdrawnState;
            /* no icon */
            atoms[1] = None;
            XChangeProperty(display, window->client.id, ATOM(WM_STATE),
                    ATOM(WM_STATE), 32, PropModeReplace,
                    (unsigned char*) atoms, 2);
        }

        unmap_client(&window->client);
    }
}
