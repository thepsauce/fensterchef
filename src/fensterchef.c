#include <inttypes.h>

#include <X11/Xatom.h>

#include "font.h"
#include "frame.h"
#include "log.h"
#include "window.h"
#include "x11_synchronize.h"

/* the home directory */
const char *Fensterchef_home;

/* the user specified path for the configuration file */
char *Fensterchef_configuration;

/* true while the window manager is running */
bool Fensterchef_is_running;

/* Spawn a window that has the `FENSTERCHEF_COMMAND` property. */
void run_external_command(const char *command)
{
    Display *display;

    XSetWindowAttributes attributes;
    Window window;

    Atom command_atom;

    XEvent event;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "fensterchef command: "
                    "could not connect to the X server\n");
        exit(EXIT_FAILURE);
    }

    attributes.event_mask = PropertyChangeMask;
    window = XCreateWindow(display, DefaultRootWindow(display), -1, -1, 1, 1, 0,
            CopyFromParent, InputOnly, (Visual*) CopyFromParent,
            CWEventMask, &attributes);

    command_atom = XInternAtom(display, "FENSTERCHEF_COMMAND", False);

    XChangeProperty(display, window, command_atom, XA_STRING, 8,
            PropModeReplace, (unsigned char*) command, strlen(command));

    XMapWindow(display, window);

    fprintf(stderr, "fensterchef command: command was dispatched, "
                "waiting until execution...\n");

    XFlush(display);

    /* wait until the property gets removed */
    while (true) {
        XNextEvent(display, &event);
        if (event.type == PropertyNotify) {
            /* if the property gets removed, it means the command was executed
             */
            if (event.xproperty.atom == command_atom &&
                    event.xproperty.state == PropertyDelete) {
                break;
            }
        }
    }

    XCloseDisplay(display);
    exit(EXIT_SUCCESS);
}

/* Close the display to the X server and exit the program with given exit
 * code.
 */
void quit_fensterchef(int exit_code)
{
    LOG("quitting fensterchef with exit code: %d\n", exit_code);
    /* when debugging, this avoids ugly messages from the sanitizer */
    free_font_list();
    XCloseDisplay(display);
    exit(exit_code);
}

/* Dump a frame into a file. */
static void dump_frame(Frame *frame, unsigned indentation, FILE *file)
{
    for (unsigned i = 0; i < indentation; i++) {
        fputs("  ", file);
    }
    fprintf(file, "%p %u ",
            (void*) frame, frame->number);
    fprintf(file, "%d %d %u %u ",
            frame->x, frame->y, frame->width, frame->height);
    fprintf(file, "%p ",
            (void*) frame->window);
    fprintf(file, "%u/%u",
            frame->ratio.numerator, frame->ratio.denominator);
    switch (frame->split_direction) {
    case FRAME_SPLIT_HORIZONTALLY:
        fputc('H', file);
        break;
    case FRAME_SPLIT_VERTICALLY:
        fputc('V', file);
        break;
    }
    fputc('\n', file);
    if (frame->left != NULL) {
        dump_frame(frame->left, indentation + 1, file);
        dump_frame(frame->right, indentation + 1, file);
    }
}

/* Output the frames and windows into a file as textual output. */
int dump_frames_and_windows(const char *file_path)
{
    FILE *file;

    file = fopen(file_path, "w");
    if (file == NULL) {
        return ERROR;
    }

    fputs("[Global]:\n", file);
    fprintf(file, "%p %p\n",
            (void*) Frame_focus, (void*) Window_focus);

    fprintf(file, "%p\n",
            (void*) Window_oldest);

    fprintf(file, "%p %p\n",
            (void*) Window_bottom, (void*) Window_top);

    fprintf(file, "%p\n",
            (void*) Window_first);

    fputs("[Frames]:\n", file);
    for (Monitor *monitor = Monitor_first;
            monitor != NULL;
            monitor = monitor->next) {
        fprintf(file, "%s ",
                monitor->name);
        fprintf(file, "%d %d %u %u\n",
                monitor->x, monitor->y, monitor->width, monitor->height);
        dump_frame(monitor->frame, 0, file);
    }

    fputs("[Stash]:\n", file);
    for (Frame *frame = Frame_last_stashed;
            frame != NULL;
            frame = frame->previous_stashed) {
        dump_frame(frame, 0, file);
    }

    fputs("[Windows]:\n", file);
    for (FcWindow *window = Window_first;
            window != NULL;
            window = window->next) {
        fprintf(file, "%s\n",
                window->name);
        fprintf(file, "%p %#lx %u",
                (void*) window, window->client.id, window->number);
        fputc(window->state.is_visible ? 'V' : 'I', file);

        switch (window->state.previous_mode) {
        case WINDOW_MODE_TILING:
            fputc('T', file);
            break;
        case WINDOW_MODE_FLOATING:
            fputc('O', file);
            break;
        case WINDOW_MODE_FULLSCREEN:
            fputc('F', file);
            break;
        case WINDOW_MODE_DOCK:
            fputc('D', file);
            break;
        case WINDOW_MODE_DESKTOP:
            fputc('P', file);
            break;
        case WINDOW_MODE_MAX:
            fputc('N', file);
            break;
        }

        switch (window->state.mode) {
        case WINDOW_MODE_TILING:
            fputc('T', file);
            break;
        case WINDOW_MODE_FLOATING:
            fputc('O', file);
            break;
        case WINDOW_MODE_FULLSCREEN:
            fputc('F', file);
            break;
        case WINDOW_MODE_DOCK:
            fputc('D', file);
            break;
        case WINDOW_MODE_DESKTOP:
            fputc('P', file);
            break;
        case WINDOW_MODE_MAX:
            fputc('N', file);
            break;
        }

        fprintf(file, " %d %d %u %u ",
                window->x, window->y, window->width, window->height);
        fprintf(file, "%u %" PRIu32 " ",
                window->border_size, window->border_color);
        fprintf(file, "%d %d %u %u ",
                window->floating.x, window->floating.y,
                window->floating.width, window->floating.height);
        fprintf(file, "%p %p %p %p\n",
                (void*) window->newer,
                (void*) window->below, (void*) window->above,
                (void*) window->next);
    }

    fclose(file);
    return OK;
}
