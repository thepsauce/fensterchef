#include <inttypes.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/Xproto.h>

#include "event.h"
#include "log.h"
#include "fensterchef.h"
#include "notification.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"
#include "x11_management.h"

/* supporting wm check window */
Window wm_check_window;

/* display to the X server */
Display *display;

/* file descriptor associated to the X display */
int x_file_descriptor;

/* Initialize the X display. */
int initialize_connection(void)
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
        return ERROR;
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

    /* initialize the X atoms */
    initialize_atoms();
    return OK;
}

/* The handler for X errors. */
static int x_error_handler(Display *display, XErrorEvent *error)
{
    char buffer[128];

    if (error->error_code == xkb_error_base) {
        LOG_ERROR("Xkb request failed: %d\n",
                error->request_code);
        return 0;
    }

    if (error->error_code == randr_error_base) {
        LOG_ERROR("Xkb request failed: %d\n",
                error->request_code);
        return 0;
    }

    /* TODO: ignore BadWindow errors because they appear even when everything is
     * fine
     */

    XGetErrorText(display, error->error_code, buffer, sizeof(buffer));

    LOG_ERROR("X error: %s\n", buffer);
    return 0;
}

/* Try to take control of the window manager role. */
void take_control(void)
{
    XSetWindowAttributes attributes;

    /* with this event mask, we get the following events:
     * SubstructureRedirectMask
     *  CirculateRequest
     *  ConfigureRequest
     *  MapRequest
     * SubstructureNotifyMask
     *  CirculateNotify
     * 	ConfigureNotify
     * 	CreateNotify
     * 	DestroyNotify
     * 	GravityNotify
     *  MapNotify
     *  ReparentNotify
     *  UnmapNotify
     */
    attributes.event_mask = SubstructureRedirectMask | SubstructureNotifyMask;
    XChangeWindowAttributes(display, DefaultRootWindow(display), CWEventMask,
            &attributes);

    /* create the wm check window, this can be used by other applications to
     * identify our window manager, we also use it as fallback focus
     */
    wm_check_window = XCreateWindow(display, DefaultRootWindow(display),
            -1, -1, 1, 1, 0, CopyFromParent, InputOnly,
            (Visual*) CopyFromParent, 0, NULL);
    /* set the title to the window manager name */
    XStoreName(display, wm_check_window, FENSTERCHEF_NAME);
    /* set the check window property so it points to itself */
    XChangeProperty(display, wm_check_window, ATOM(_NET_SUPPORTING_WM_CHECK),
            XA_WINDOW, 32, PropModeReplace,
            (unsigned char*) &wm_check_window, 1);
    /* map the window so it can receive the fallback focus */
    XMapWindow(display, wm_check_window);

    /* initialize the focus */
    XSetInputFocus(display, wm_check_window, RevertToParent, CurrentTime);

    /* wait until we get replies to our requests, all should succeed */
    XSync(display, False);

    /* set an error handler so that further errors do not abort fensterchef */
    XSetErrorHandler(x_error_handler);

    Fensterchef_is_running = true;
}

/* Go through all existing windows and manage them. */
void query_existing_windows(void)
{
    Window root;
    Window parent;
    Window *children;
    unsigned number_of_children;

    /* get a list of child windows of the root in bottom-to-top stacking order
     */
    XQueryTree(display, DefaultRootWindow(display), &root, &parent, &children,
            &number_of_children);

    for (unsigned i = 0; i < number_of_children; i++) {
        (void) create_window(children[i]);
    }

    XFree(children);
}

/* Set the input focus to @window. This window may be `NULL`. */
void set_input_focus(FcWindow *window)
{
    Window focus_id = None;
    Window active_id;
    Atom state_atom;

    if (window == NULL) {
        LOG("removed focus from all windows\n");
        active_id = DefaultRootWindow(display);

        focus_id = wm_check_window;
    } else {
        active_id = window->client.id;

        state_atom = ATOM(_NET_WM_STATE_FOCUSED);
        add_window_states(window, &state_atom, 1);

        /* if the window wants no focus itself */
        if ((window->hints.flags & InputHint) && window->hints.input == 0) {
            XEvent event;

            memset(&event, 0, sizeof(event));
            /* bake an event for running a protocol on the window */
            event.type = ClientMessage;
            event.xclient.window = window->client.id;
            event.xclient.message_type = ATOM(WM_PROTOCOLS);
            event.xclient.format = 32;
            event.xclient.data.l[0] = ATOM(WM_TAKE_FOCUS);
            event.xclient.data.l[1] = CurrentTime;
            XSendEvent(display, window->client.id, false, NoEventMask, &event);

            LOG("focusing client: %w through sending WM_TAKE_FOCUS\n",
                    window->client.id);
        } else {
            focus_id = window->client.id;
        }
    }

    if (focus_id != None) {
        LOG("focusing client: %w\n", focus_id);
        XSetInputFocus(display, focus_id, RevertToParent, CurrentTime);
    }

    /* set the active window root property */
    XChangeProperty(display, DefaultRootWindow(display),
            ATOM(_NET_ACTIVE_WINDOW), XA_WINDOW, 32, PropModeReplace,
            (unsigned char*) &active_id, 1);
}

/* Show the client on the X server. */
void map_client(XClient *client)
{
    if (client->is_mapped) {
        return;
    }

    LOG("showing client %w\n", client->id);

    client->is_mapped = true;

    XMapWindow(display, client->id);
}

/* Show the client on the X server at the top. */
void map_client_raised(XClient *client)
{
    if (client->is_mapped) {
        return;
    }

    LOG("showing client %w raised\n", client->id);

    client->is_mapped = true;

    XMapRaised(display, client->id);
}

/* Hide the client on the X server. */
void unmap_client(XClient *client)
{
    if (!client->is_mapped) {
        return;
    }

    LOG("hiding client %w\n", client->id);

    client->is_mapped = false;

    XUnmapWindow(display, client->id);
}

/* Set the size of a window associated to the X server. */
void configure_client(XClient *client, int32_t x, int32_t y, uint32_t width,
        uint32_t height, uint32_t border_width)
{
    XWindowChanges changes;
    unsigned mask = 0;

    if (client->x != x) {
        client->x = x;
        changes.x = x;
        mask |= CWX;
    }

    if (client->y != y) {
        client->y = y;
        changes.y = y;
        mask |= CWY;
    }

    if (client->width != width) {
        client->width = width;
        changes.width = width;
        mask |= CWWidth;
    }

    if (client->height != height) {
        client->height = height;
        changes.height = height;
        mask |= CWHeight;
    }

    if (client->border_width != border_width) {
        client->border_width = border_width;
        changes.border_width = border_width;
        mask |= CWBorderWidth;
    }

    if (mask != 0) {
        LOG("configuring client %w to %R %u\n",
                client->id, x, y, width, height, border_width);
        XConfigureWindow(display, client->id, mask, &changes);
    }
}

/* Set the client border color. */
void change_client_attributes(XClient *client, uint32_t background_color,
        uint32_t border_color)
{
    XSetWindowAttributes attributes;
    unsigned mask = 0;

    if (client->background != background_color) {
        client->background = background_color;
        attributes.backing_pixel = background_color;
        mask |= CWBackPixel;
    }

    if (client->border != border_color) {
        client->border = border_color;
        attributes.border_pixel = border_color;
        mask |= CWBorderPixel;
    }

    if (mask != 0) {
        LOG("changing attributes of client %w to " COLOR(GREEN) "#%08x, "
                    COLOR(GREEN) "#%08x\n",
                client->id, background_color, border_color);
        XChangeWindowAttributes(display, client->id, mask, &attributes);
    }
}
