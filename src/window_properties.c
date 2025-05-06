#include <string.h>

#include "configuration/configuration.h"
#include "log.h"
#include "utility/utility.h"
#include "window.h"
#include "window_properties.h"

/* all X atom names */
const char *x_atom_names[ATOM_MAX] = {
#define X(atom) \
    #atom,
    DEFINE_ALL_ATOMS
#undef X
};

/* all X atom ids */
Atom x_atom_ids[ATOM_MAX];

/* Initialize all X atoms. */
void initialize_atoms(void)
{
    XInternAtoms(display, (char**) x_atom_names, ATOM_MAX, False, x_atom_ids);
}

/* Set the initial root window properties. */
void initialize_root_properties(void)
{
    Window root;

    /* set the supported ewmh atoms of our window manager, ewmh support was easy
     * to add with the help of:
     * https://specifications.freedesktop.org/wm-spec/latest/index.html#id-1.2
     */
    const Atom supported_atoms[] = {
        ATOM(_NET_SUPPORTED),

        ATOM(_NET_CLIENT_LIST),
        ATOM(_NET_CLIENT_LIST_STACKING),

        ATOM(_NET_ACTIVE_WINDOW),

        ATOM(_NET_SUPPORTING_WM_CHECK),

        ATOM(_NET_CLOSE_WINDOW),

        ATOM(_NET_MOVERESIZE_WINDOW),

        ATOM(_NET_WM_MOVERESIZE),

        ATOM(_NET_RESTACK_WINDOW),

        ATOM(_NET_REQUEST_FRAME_EXTENTS),

        ATOM(_NET_WM_NAME),

        ATOM(_NET_WM_DESKTOP),

        ATOM(_NET_WM_WINDOW_TYPE),
        ATOM(_NET_WM_WINDOW_TYPE_DESKTOP),
        ATOM(_NET_WM_WINDOW_TYPE_DOCK),
        ATOM(_NET_WM_WINDOW_TYPE_TOOLBAR),
        ATOM(_NET_WM_WINDOW_TYPE_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_UTILITY),
        ATOM(_NET_WM_WINDOW_TYPE_SPLASH),
        ATOM(_NET_WM_WINDOW_TYPE_DIALOG),
        ATOM(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_POPUP_MENU),
        ATOM(_NET_WM_WINDOW_TYPE_TOOLTIP),
        ATOM(_NET_WM_WINDOW_TYPE_NOTIFICATION),
        ATOM(_NET_WM_WINDOW_TYPE_COMBO),
        ATOM(_NET_WM_WINDOW_TYPE_DND),
        ATOM(_NET_WM_WINDOW_TYPE_NORMAL),

        ATOM(_NET_WM_STATE),
        ATOM(_NET_WM_STATE_MAXIMIZED_VERT),
        ATOM(_NET_WM_STATE_MAXIMIZED_HORZ),
        ATOM(_NET_WM_STATE_FULLSCREEN),
        ATOM(_NET_WM_STATE_HIDDEN),
        ATOM(_NET_WM_STATE_FOCUSED),

        ATOM(_NET_WM_STRUT),
        ATOM(_NET_WM_STRUT_PARTIAL),

        ATOM(_NET_FRAME_EXTENTS),

        ATOM(_NET_WM_FULLSCREEN_MONITORS),
    };
    root = DefaultRootWindow(display);
    XChangeProperty(display, root, ATOM(_NET_SUPPORTED), XA_ATOM, 32,
            PropModeReplace, (unsigned char*) supported_atoms,
            SIZE(supported_atoms));

    /* the wm check window */
    XChangeProperty(display, root, ATOM(_NET_SUPPORTING_WM_CHECK), XA_WINDOW,
            32, PropModeReplace, (unsigned char*) &wm_check_window, 1);


    /* set the active window */
    XChangeProperty(display, root, ATOM(_NET_ACTIVE_WINDOW), XA_WINDOW, 32,
            PropModeReplace, (unsigned char*) &root, 1);
}

static inline long *get_long_property(
        Window window, Atom property, unsigned long expected_item_count)
{
    Atom type;
    int format;
    unsigned long item_count;
    unsigned long bytes_after;
    unsigned char *property_result;

    XGetWindowProperty(display, window, property, 0, expected_item_count, False,
            AnyPropertyType, &type, &format, &item_count, &bytes_after,
            &property_result);
    if (format != 32 || item_count != expected_item_count) {
        if (type != None) {
            LOG("window %w has misformatted property %a\n",
                    window, property);
        }
        XFree(property_result);
        property_result = NULL;
    }
    return (long*) property_result;
}

static inline char *get_text_property(Window window, Atom property,
        unsigned long *length)
{
    Atom type;
    int format;
    unsigned long bytes_after;
    unsigned char *property_result;

    XGetWindowProperty(display, window, property, 0, 1024, False,
            AnyPropertyType, &type, &format, length,
            &bytes_after, &property_result);

    if (format != 8) {
        if (type != None) {
            LOG("window %w has misformatted property %a\n",
                    window, property_result);
        }
        XFree(property_result);
        property_result = NULL;
    }

    return (char*) property_result;
}

/* Gets the `FENSTERCHEF_COMMAND` property from @window. */
char *get_fensterchef_command_property(Window window)
{
    char *command_property;
    unsigned long length;
    char *command = NULL;

    command_property = get_text_property(window, ATOM(FENSTERCHEF_COMMAND),
            &length);
    if (command_property != NULL) {
        command = xstrndup(command_property, length);
        XFree(command_property);
    }
    return command;
}

/* Update the name within @properties. */
static void update_window_name(FcWindow *window)
{
    char *name;
    unsigned long length;

    free(window->name);
    window->name = NULL;

    name = get_text_property(window->client.id, ATOM(_NET_WM_NAME), &length);
    /* try to fall back to `WM_NAME` */
    if (name == NULL) {
        name = get_text_property(window->client.id, XA_WM_NAME, &length);
    }

    if (name != NULL) {
        window->name = xstrndup(name, length);
        free(name);
    }
}

/* Update the size_hints within @properties. */
static void update_window_normal_hints(FcWindow *window)
{
    long supplied;

    XGetWMNormalHints(display, window->client.id, &window->size_hints,
            &supplied);
}

/* Update the hints within @properties. */
static void update_window_hints(FcWindow *window)
{
    XWMHints *wm_hints;

    wm_hints = XGetWMHints(display, window->client.id);
    if (wm_hints == NULL) {
        window->hints.flags = 0;
    } else {
        window->hints = *wm_hints;
        XFree(wm_hints);
    }
}

/* Update the strut partial property within @properties. */
static void update_window_strut(FcWindow *window)
{
    long *strut;

    memset(&window->strut, 0, sizeof(window->strut));

    strut = get_long_property(window->client.id, ATOM(_NET_WM_STRUT_PARTIAL),
            sizeof(wm_strut_partial_t) / sizeof(int));
    if (strut == NULL) {
        /* `_NET_WM_STRUT` is older than `_NET_WM_STRUT_PARTIAL`, fall back to
         * it when there is no strut partial
         */
        strut = get_long_property(window->client.id, ATOM(_NET_WM_STRUT), 4);
        if (strut != NULL) {
            window->strut.left = strut[0];
            window->strut.top = strut[1];
            window->strut.right = strut[2];
            window->strut.bottom = strut[3];
        }
    } else {
        window->strut.left = strut[0];
        window->strut.top = strut[1];
        window->strut.right = strut[2];
        window->strut.bottom = strut[3];
        window->strut.left_start_y = strut[4];
        window->strut.left_end_y = strut[5];
        window->strut.right_start_y = strut[6];
        window->strut.right_end_y = strut[7];
        window->strut.top_start_x = strut[8];
        window->strut.top_end_x = strut[9];
        window->strut.bottom_start_x = strut[10];
        window->strut.bottom_end_x = strut[11];
    }

    XFree(strut);
}

/* Get a window property as list of atoms. */
static inline Atom *get_atom_list_property(Window window, Atom property)
{
    Atom type;
    int format;
    unsigned long count;
    unsigned long bytes_after;
    unsigned char *raw_property;
    Atom *raw_atoms;
    Atom *atoms;

    /* get up to 32 atoms from this property */
    XGetWindowProperty(display, window, property, 0, 32, False,
            XA_ATOM, &type, &format, &count,
            &bytes_after, &raw_property);
    if (format != 32 || type != XA_ATOM) {
        if (type != None) {
            LOG("window %w has misformatted property %a\n",
                    window, property);
        }
        XFree(raw_property);
        return NULL;
    }

    raw_atoms = (Atom*) raw_property;
    atoms = xmalloc((count + 1) * sizeof(*atoms));
    memcpy(atoms, raw_atoms, count * sizeof(*atoms));
    atoms[count] = None;
    XFree(raw_property);
    return atoms;
}

/* Update the `transient_for` property within @window. */
static void update_window_transient_for(FcWindow *window)
{
    XGetTransientForHint(display, window->client.id, &window->transient_for);
}

/* Update the `protocols` property within @window. */
static void update_window_protocols(FcWindow *window)
{
    free(window->protocols);
    window->protocols = get_atom_list_property(window->client.id,
            ATOM(WM_PROTOCOLS));
}

/* Update the `fullscreen_monitors` property within @window. */
static void update_window_fullscreen_monitors(FcWindow *window)
{
    long *monitors;

    monitors = get_long_property(window->client.id,
            ATOM(_NET_WM_FULLSCREEN_MONITORS),
            sizeof(window->fullscreen_monitors) / sizeof(int));
    if (monitors == NULL) {
        memset(&window->fullscreen_monitors, 0,
                sizeof(window->fullscreen_monitors));
    } else {
        window->fullscreen_monitors.left = monitors[0];
        window->fullscreen_monitors.top = monitors[1];
        window->fullscreen_monitors.right = monitors[2];
        window->fullscreen_monitors.bottom = monitors[3];
        XFree(monitors);
    }
}

/* Update `is_borderless` within @window base on `_MOTIF_WM_HINTS`. */
static void update_motif_wm_hints(FcWindow *window)
{
    long *motif_wm_hints;
    struct motif_wm_hints {
        /* what fields below are available */
        int flags;
        /* IGNORED */
        int functions;
        /* border/frame decoration flags */
        int decorations;
        /* IGNORED */
        int input_mode;
        /* IGNORED */
        int status;
    } hints;

    const int decorations_flag = (1 << 1);
    const int decorate_all = (1 << 0);
    const int decorate_border = (1 << 2) | (1 << 3);

    window->is_borderless = false;

    motif_wm_hints = get_long_property(window->client.id, ATOM(_MOTIF_WM_HINTS),
            sizeof(hints) / sizeof(int));
    if (motif_wm_hints != NULL) {
        hints.flags = motif_wm_hints[0];
        hints.decorations = motif_wm_hints[2];
        XFree(motif_wm_hints);

        if ((hints.flags & decorations_flag)) {
            /* if `decorate_all` is set, the other flags are exclusive */
            if ((hints.decorations & decorate_all)) {
                if ((hints.decorations & decorate_border)) {
                    window->is_borderless = true;
                }
            } else if (!(hints.decorations & decorate_border)) {
                window->is_borderless = true;
            }
        }
    }
}

/* Update the property within @window corresponding to given atom. */
bool cache_window_property(FcWindow *window, Atom atom)
{
    /* this is spaced out because it was very difficult to read with the eyes */
    if (atom == XA_WM_NAME || atom == ATOM(_NET_WM_NAME)) {

        update_window_name(window);

    } else if (atom == XA_WM_NORMAL_HINTS) {

        update_window_normal_hints(window);

    } else if (atom == XA_WM_HINTS) {

        update_window_hints(window);

    } else if (atom == ATOM(_NET_WM_STRUT) ||
            atom == ATOM(_NET_WM_STRUT_PARTIAL)) {

        update_window_strut(window);

    } else if (atom == XA_WM_TRANSIENT_FOR) {

        update_window_transient_for(window);

    } else if (atom == ATOM(WM_PROTOCOLS)) {

        update_window_protocols(window);

    } else if (atom == ATOM(_NET_WM_FULLSCREEN_MONITORS)) {

        update_window_fullscreen_monitors(window);

    } else if (atom == ATOM(_MOTIF_WM_HINTS)) {

        update_motif_wm_hints(window);

    } else {
        return false;
    }
    return true;
}

/* Check if an atom is within the given list of atoms. */
static bool is_atom_included(const Atom *atoms, Atom atom)
{
    if (atoms == NULL) {
        return false;
    }
    for (; atoms[0] != None; atoms++) {
        if (atoms[0] == atom) {
            return true;
        }
    }
    return false;
}

/* Initialize all properties within @properties. */
const struct configuration_association *initialize_window_properties(
        FcWindow *window, window_mode_t *mode)
{
    int atom_count;
    Atom *atoms;
    char *wm_class;
    unsigned long wm_class_length;
    unsigned long instance_length;
    utf8_t *instance_name = NULL;
    utf8_t *class_name = NULL;
    Atom *states = NULL;
    Atom *types = NULL;
    window_mode_t predicted_mode = WINDOW_MODE_TILING;
    const struct configuration_association *matching_association = NULL;

    /* get a list of properties currently set on the window */
    atoms = XListProperties(display, window->client.id, &atom_count);

    /* cache all properties */
    for (int i = 0; i < atom_count; i++) {
        if (atoms[i] == XA_WM_CLASS) {
            wm_class = get_text_property(window->client.id, XA_WM_CLASS,
                    &wm_class_length);

            instance_length = strnlen(wm_class, wm_class_length);
            instance_name = xmalloc(instance_length + 1);
            memcpy(instance_name, wm_class, instance_length);
            instance_name[instance_length] = '\0';

            class_name = xstrndup(
                    &wm_class[instance_length + 1],
                    wm_class_length - instance_length);

            XFree(wm_class);
        } else if (atoms[i] == ATOM(_NET_WM_STATE)) {
            states = get_atom_list_property(window->client.id,
                    ATOM(_NET_WM_STATE));
        } else if (atoms[i] == ATOM(_NET_WM_WINDOW_TYPE)) {
            types = get_atom_list_property(window->client.id,
                    ATOM(_NET_WM_WINDOW_TYPE));
        } else {
            cache_window_property(window, atoms[i]);
        }
    }

    /* these are three direct checks */
    if (is_atom_included(states, ATOM(_NET_WM_STATE_FULLSCREEN)) ||
            is_atom_included(states, ATOM(_NET_WM_STATE_MAXIMIZED_HORZ)) ||
            is_atom_included(states, ATOM(_NET_WM_STATE_MAXIMIZED_VERT))) {
        predicted_mode = WINDOW_MODE_FULLSCREEN;
    } else if (is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_DOCK))) {
        predicted_mode = WINDOW_MODE_DOCK;
    } else if (is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_DESKTOP))) {
        predicted_mode = WINDOW_MODE_DESKTOP;
    /* if this window has strut, it must be a dock window */
    } else if (!is_strut_empty(&window->strut)) {
        predicted_mode = WINDOW_MODE_DOCK;
    /* transient windows are floating windows */
    } else if (window->transient_for != 0) {
        predicted_mode = WINDOW_MODE_FLOATING;
    /* floating windows have an equal minimum and maximum size */
    } else if ((window->size_hints.flags & (PMinSize | PMaxSize)) ==
            (PMinSize | PMaxSize) &&
            (window->size_hints.min_width == window->size_hints.max_width ||
             window->size_hints.min_height == window->size_hints.max_height)) {
        predicted_mode = WINDOW_MODE_FLOATING;
    /* floating windows have a window type that is not the normal window type */
    } else if (types != NULL &&
            !is_atom_included(types, ATOM(_NET_WM_WINDOW_TYPE_NORMAL))) {
        predicted_mode = WINDOW_MODE_FLOATING;
    }

    window->states = states;

    /* if the class property is set, try to find an association */
    if (instance_name != NULL && class_name != NULL) {
        for (size_t i = 0; i < configuration.number_of_associations; i++) {
            const struct configuration_association *const association =
                &configuration.associations[i];
            if ((association->instance_pattern == NULL ||
                        matches_pattern(association->instance_pattern,
                            instance_name)) &&
                    matches_pattern(association->class_pattern, class_name)) {
                matching_association = association;
                break;
            }
        }
    }

    free(instance_name);
    free(class_name);
    free(types);

    XFree(atoms);

    *mode = predicted_mode;
    return matching_association;
}

/* Check if @properties includes @protocol. */
bool supports_protocol(FcWindow *window, Atom protocol)
{
    return is_atom_included(window->protocols, protocol);
}

/* Check if @properties includes @state. */
bool has_state(FcWindow *window, Atom state)
{
    return is_atom_included(window->states, state);
}
