#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <xcb/randr.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_event.h>

#include "action.h"
#include "event.h"
#include "frame.h"
#include "log.h"
#include "window.h"
#include "window_list.h"
#include "window_properties.h"
#include "x11_management.h"

/* the severity of the logging */
log_severity_t log_severity = LOG_SEVERITY_INFO;

/***********************/
/** String conversion **/

static const char *generic_event_strings[] = {
    [XCB_REQUEST] = "REQUEST",
    [XCB_VALUE] = "VALUE",
    [XCB_WINDOW] = "WINDOW",
    [XCB_PIXMAP] = "PIXMAP",
    [XCB_ATOM] = "ATOM",
    [XCB_CURSOR] = "CURSOR",
    [XCB_FONT] = "FONT",
    [XCB_MATCH] = "MATCH",
    [XCB_DRAWABLE] = "DRAWABLE",
    [XCB_ACCESS] = "ACCESS",
    [XCB_ALLOC] = "ALLOC",
    [XCB_COLORMAP] = "COLORMAP",
    [XCB_G_CONTEXT] = "G_CONTEXT",
    [XCB_ID_CHOICE] = "ID_CHOICE",
    [XCB_NAME] = "NAME",
    [XCB_LENGTH] = "LENGTH",
    [XCB_IMPLEMENTATION] = "IMPLEMENTATION",
};

static const char *atom_to_string(xcb_atom_t atom)
{
    const char *xcb_atoms[] = {
        [XCB_ATOM_NONE] = "NONE",
        [XCB_ATOM_PRIMARY] = "PRIMARY",
        [XCB_ATOM_SECONDARY] = "SECONDARY",
        [XCB_ATOM_ARC] = "ARC",
        [XCB_ATOM_ATOM] = "ATOM",
        [XCB_ATOM_BITMAP] = "BITMAP",
        [XCB_ATOM_CARDINAL] = "CARDINAL",
        [XCB_ATOM_COLORMAP] = "COLORMAP",
        [XCB_ATOM_CURSOR] = "CURSOR",
        [XCB_ATOM_CUT_BUFFER0] = "CUT_BUFFER0",
        [XCB_ATOM_CUT_BUFFER1] = "CUT_BUFFER1",
        [XCB_ATOM_CUT_BUFFER2] = "CUT_BUFFER2",
        [XCB_ATOM_CUT_BUFFER3] = "CUT_BUFFER3",
        [XCB_ATOM_CUT_BUFFER4] = "CUT_BUFFER4",
        [XCB_ATOM_CUT_BUFFER5] = "CUT_BUFFER5",
        [XCB_ATOM_CUT_BUFFER6] = "CUT_BUFFER6",
        [XCB_ATOM_CUT_BUFFER7] = "CUT_BUFFER7",
        [XCB_ATOM_DRAWABLE] = "DRAWABLE",
        [XCB_ATOM_FONT] = "FONT",
        [XCB_ATOM_INTEGER] = "INTEGER",
        [XCB_ATOM_PIXMAP] = "PIXMAP",
        [XCB_ATOM_POINT] = "POINT",
        [XCB_ATOM_RECTANGLE] = "RECTANGLE",
        [XCB_ATOM_RESOURCE_MANAGER] = "RESOURCE_MANAGER",
        [XCB_ATOM_RGB_COLOR_MAP] = "RGB_COLOR_MAP",
        [XCB_ATOM_RGB_BEST_MAP] = "RGB_BEST_MAP",
        [XCB_ATOM_RGB_BLUE_MAP] = "RGB_BLUE_MAP",
        [XCB_ATOM_RGB_DEFAULT_MAP] = "RGB_DEFAULT_MAP",
        [XCB_ATOM_RGB_GRAY_MAP] = "RGB_GRAY_MAP",
        [XCB_ATOM_RGB_GREEN_MAP] = "RGB_GREEN_MAP",
        [XCB_ATOM_RGB_RED_MAP] = "RGB_RED_MAP",
        [XCB_ATOM_STRING] = "STRING",
        [XCB_ATOM_VISUALID] = "VISUALID",
        [XCB_ATOM_WINDOW] = "WINDOW",
        [XCB_ATOM_WM_COMMAND] = "WM_COMMAND",
        [XCB_ATOM_WM_HINTS] = "WM_HINTS",
        [XCB_ATOM_WM_CLIENT_MACHINE] = "WM_CLIENT_MACHINE",
        [XCB_ATOM_WM_ICON_NAME] = "WM_ICON_NAME",
        [XCB_ATOM_WM_ICON_SIZE] = "WM_ICON_SIZE",
        [XCB_ATOM_WM_NAME] = "WM_NAME",
        [XCB_ATOM_WM_NORMAL_HINTS] = "WM_NORMAL_HINTS",
        [XCB_ATOM_WM_SIZE_HINTS] = "WM_SIZE_HINTS",
        [XCB_ATOM_WM_ZOOM_HINTS] = "WM_ZOOM_HINTS",
        [XCB_ATOM_MIN_SPACE] = "MIN_SPACE",
        [XCB_ATOM_NORM_SPACE] = "NORM_SPACE",
        [XCB_ATOM_MAX_SPACE] = "MAX_SPACE",
        [XCB_ATOM_END_SPACE] = "END_SPACE",
        [XCB_ATOM_SUPERSCRIPT_X] = "SUPERSCRIPT_X",
        [XCB_ATOM_SUPERSCRIPT_Y] = "SUPERSCRIPT_Y",
        [XCB_ATOM_SUBSCRIPT_X] = "SUBSCRIPT_X",
        [XCB_ATOM_SUBSCRIPT_Y] = "SUBSCRIPT_Y",
        [XCB_ATOM_UNDERLINE_POSITION] = "UNDERLINE_POSITION",
        [XCB_ATOM_UNDERLINE_THICKNESS] = "UNDERLINE_THICKNESS",
        [XCB_ATOM_STRIKEOUT_ASCENT] = "STRIKEOUT_ASCENT",
        [XCB_ATOM_STRIKEOUT_DESCENT] = "STRIKEOUT_DESCENT",
        [XCB_ATOM_ITALIC_ANGLE] = "ITALIC_ANGLE",
        [XCB_ATOM_X_HEIGHT] = "X_HEIGHT",
        [XCB_ATOM_QUAD_WIDTH] = "QUAD_WIDTH",
        [XCB_ATOM_WEIGHT] = "WEIGHT",
        [XCB_ATOM_POINT_SIZE] = "POINT_SIZE",
        [XCB_ATOM_RESOLUTION] = "RESOLUTION",
        [XCB_ATOM_COPYRIGHT] = "COPYRIGHT",
        [XCB_ATOM_NOTICE] = "NOTICE",
        [XCB_ATOM_FONT_NAME] = "FONT_NAME",
        [XCB_ATOM_FAMILY_NAME] = "FAMILY_NAME",
        [XCB_ATOM_FULL_NAME] = "FULL_NAME",
        [XCB_ATOM_CAP_HEIGHT] = "CAP_HEIGHT",
        [XCB_ATOM_WM_CLASS] = "WM_CLASS",
        [XCB_ATOM_WM_TRANSIENT_FOR] = "WM_TRANSIENT_FOR",
    };

    if (atom < SIZE(xcb_atoms)) {
        return xcb_atoms[atom];
    }
    for (uint32_t i = 0; i < SIZE(x_atoms); i++) {
        if (x_atoms[i].atom == atom) {
            return x_atoms[i].name;
        }
    }

    return NULL;
}

static const char *notify_detail_to_string(xcb_notify_detail_t detail)
{
    switch (detail) {
    case XCB_NOTIFY_DETAIL_ANCESTOR:
        return "ancestor";
    case XCB_NOTIFY_DETAIL_VIRTUAL:
        return "virtual";
    case XCB_NOTIFY_DETAIL_INFERIOR:
        return "inferior";
    case XCB_NOTIFY_DETAIL_NONLINEAR:
        return "nonlinear";
    case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
        return "nonlinear virtual";
    case XCB_NOTIFY_DETAIL_POINTER:
        return "pointer";
    case XCB_NOTIFY_DETAIL_POINTER_ROOT:
        return "pointer root";
    case XCB_NOTIFY_DETAIL_NONE:
        return "none";
    default:
        return NULL;
    }
}

static const char *notify_mode_to_string(xcb_notify_mode_t mode)
{
    switch (mode) {
    case XCB_NOTIFY_MODE_NORMAL:
        return "normal";
    case XCB_NOTIFY_MODE_GRAB:
        return "grab";
    case XCB_NOTIFY_MODE_UNGRAB:
        return "ungrab";
    case XCB_NOTIFY_MODE_WHILE_GRABBED:
        return "while grabbed";
    default:
        return NULL;
    }
}

static const char *button_to_string(xcb_button_t button)
{
    switch (button) {
    case XCB_BUTTON_INDEX_ANY:
        return "any";
    case XCB_BUTTON_INDEX_1:
        return "left";
    case XCB_BUTTON_INDEX_2:
        return "middle";
    case XCB_BUTTON_INDEX_3:
        return "right";
    case XCB_BUTTON_INDEX_4:
        return "scroll up";
    case XCB_BUTTON_INDEX_5:
        return "scroll down";
    case /* XCB_BUTTON_INDEX_6 */ 6:
        return "scroll left";
    case /* XCB_BUTTON_INDEX_7 */ 7:
        return "scroll right";
    default:
        return NULL;
    }
}

static const char *direction_to_string(wm_move_resize_direction_t direction)
{
    switch (direction) {
    case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
        return "top left";
    case _NET_WM_MOVERESIZE_SIZE_TOP:
        return "top";
    case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
        return "top right";
    case _NET_WM_MOVERESIZE_SIZE_RIGHT:
        return "right";
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
        return "bottom right";
    case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
        return "bottom";
    case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
        return "bottom left";
    case _NET_WM_MOVERESIZE_SIZE_LEFT:
        return "left";
    case _NET_WM_MOVERESIZE_MOVE:
        return "move";
    case _NET_WM_MOVERESIZE_SIZE_KEYBOARD:
        return "size keyboard";
    case _NET_WM_MOVERESIZE_MOVE_KEYBOARD:
        return "move keyboard";
    case _NET_WM_MOVERESIZE_CANCEL:
        return "cancel";
    default:
        return NULL;
    }
}

static const char *gravity_to_string(xcb_gravity_t gravity)
{
    switch (gravity) {
    case XCB_GRAVITY_WIN_UNMAP:
        return "win unmap";
    case XCB_GRAVITY_NORTH_WEST:
        return "north west";
    case XCB_GRAVITY_NORTH:
        return "north";
    case XCB_GRAVITY_NORTH_EAST:
        return "north east";
    case XCB_GRAVITY_WEST:
        return "west";
    case XCB_GRAVITY_CENTER:
        return "center";
    case XCB_GRAVITY_EAST:
        return "east";
    case XCB_GRAVITY_SOUTH_WEST:
        return "south west";
    case XCB_GRAVITY_SOUTH:
        return "south";
    case XCB_GRAVITY_SOUTH_EAST:
        return "south east";
    case XCB_GRAVITY_STATIC:
        return "static";
    default:
        return NULL;
    }
}

/****************************/
/** Utility log functions **/

static void log_boolean(bool boolean)
{
    fputs(boolean ? COLOR(GREEN) "true" CLEAR_COLOR :
            COLOR(RED) "false" CLEAR_COLOR, stderr);
}

static void log_hexadecimal(uint32_t x)
{
    fprintf(stderr, COLOR(GREEN) "%#" PRIx32 CLEAR_COLOR, x);
}

static void log_point(int32_t x, int32_t y)
{
    fprintf(stderr, COLOR(GREEN) "%" PRId32 "+%" PRId32 CLEAR_COLOR, x, y);
}

static void log_rectangle(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    fprintf(stderr, COLOR(GREEN) "%" PRId32 "+%" PRId32
                "+%" PRIu32 "x%" PRIu32  CLEAR_COLOR,
            x, y, width, height);
}

static void log_size(uint32_t width, uint32_t height)
{
    fprintf(stderr, COLOR(GREEN) "%" PRId32 "x%" PRId32 CLEAR_COLOR,
            width, height);
}

static void log_integer(int32_t x)
{
    fprintf(stderr, COLOR(GREEN) "%" PRId32 CLEAR_COLOR, x);
}

static void log_unsigned(uint32_t x)
{
    fprintf(stderr, COLOR(GREEN) "%" PRIu32 CLEAR_COLOR, x);
}

static void log_unsigned_pair(uint32_t x, uint32_t y)
{
    fprintf(stderr, COLOR(GREEN) "%" PRIu32 ",%" PRIu32 CLEAR_COLOR, x, y);
}

static void log_button(xcb_button_t button)
{
    const char *string;

    string = button_to_string(button);
    if (string == NULL) {
        fprintf(stderr, COLOR(CYAN) "X%u" CLEAR_COLOR, button - 7);
    } else {
        fprintf(stderr, COLOR(CYAN) "%s" CLEAR_COLOR, string);
    }
}

static void log_source(uint32_t source)
{
    switch (source) {
    case 1:
        fputs(COLOR(CYAN) "client" CLEAR_COLOR, stderr);
        break;
    case 2:
        fputs(COLOR(CYAN) "pager" CLEAR_COLOR, stderr);
        break;
    default:
        fputs(COLOR(CYAN) "legacy" CLEAR_COLOR, stderr);
        break;
    }
}

static void log_gravity(xcb_gravity_t gravity)
{
    const char *string;

    string = gravity_to_string(gravity);
    if (string == NULL) {
        fprintf(stderr, COLOR(GREEN) "%u" CLEAR_COLOR, gravity);
    } else {
        fprintf(stderr, COLOR(CYAN) "%s" CLEAR_COLOR, string);
    }
}

static void log_direction(wm_move_resize_direction_t direction)
{
    const char *string;

    string = direction_to_string(direction);
    if (string == NULL) {
        fprintf(stderr, COLOR(GREEN) "%u" CLEAR_COLOR, direction);
    } else {
        fprintf(stderr, COLOR(CYAN) "%s" CLEAR_COLOR, string);
    }
}

static void log_modifiers(uint32_t mask)
{
    static const char *modifiers[] = {
        "Shift", "Lock", "Ctrl",
        "Mod1", "Mod2", "Mod3", "Mod4", "Mod5",
        "Button1", "Button2", "Button3", "Button4", "Button5"
    };
    int modifier_count = 0;

    fputs(COLOR(MAGENTA), stderr);
    for (const char **modifier = modifiers; mask != 0; mask >>= 1, modifier++) {
        if ((mask & 1)) {
            if (modifier_count > 0) {
                fputs(CLEAR_COLOR "+" COLOR(MAGENTA), stderr);
            }
            fputs(*modifier, stderr);
            modifier_count++;
        }
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_xcb_window(xcb_window_t xcb_window)
{
    log_hexadecimal(xcb_window);
    fputs(COLOR(YELLOW), stderr);
    if (xcb_window == wm_check_window) {
        fputs("<check>", stderr);
    } else if (xcb_window == window_list.client.id) {
        fputs("<window list>", stderr);
    } else if (xcb_window == notification.id) {
        fputs("<notification>", stderr);
    } else if (xcb_window == screen->root) {
        fputs("<root>", stderr);
    } else {
        for (Window *window = Window_first; window != NULL;
                window = window->next) {
            if (window->client.id == xcb_window) {
                fprintf(stderr, "<%" PRIu32 ">", window->number);
                break;
            }
        }
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_motion(xcb_motion_t motion)
{
    fputs(COLOR(CYAN), stderr);
    switch (motion) {
    case XCB_MOTION_NORMAL:
        fputs("normal", stderr);
        break;
    case XCB_MOTION_HINT:
        fputs("hint", stderr);
        break;
    default:
        fprintf(stderr, "%u", motion);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_notify_detail(xcb_notify_detail_t detail)
{
    const char *string;

    fputs(COLOR(CYAN), stderr);
    string = notify_detail_to_string(detail);
    if (string == NULL) {
        fprintf(stderr, "%u", detail);
    } else {
        fputs(string, stderr);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_notify_mode(xcb_notify_mode_t mode)
{
    const char *string;

    fputs(COLOR(CYAN), stderr);
    string = notify_mode_to_string(mode);
    if (string == NULL) {
        fprintf(stderr, "%u", mode);
    } else {
        fputs(string, stderr);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_place(xcb_place_t place)
{
    fputs(COLOR(CYAN), stderr);
    switch (place) {
    case XCB_PLACE_ON_TOP:
        fputs("on top", stderr);
        break;
    case XCB_PLACE_ON_BOTTOM:
        fputs("on bottom", stderr);
        break;
    default:
        fprintf(stderr, "%u", place);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_property_state(uint8_t state)
{
    fputs(COLOR(CYAN), stderr);
    switch (state) {
    case XCB_PROPERTY_NEW_VALUE:
        fputs("new value", stderr);
        break;
    case XCB_PROPERTY_DELETE:
        fputs("delete", stderr);
        break;
    default:
        fprintf(stderr, "%" PRIu8, state);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_atom(xcb_atom_t atom)
{
    const char *atom_string;
    xcb_get_atom_name_cookie_t name_cookie;
    xcb_get_atom_name_reply_t *name;

    fputs(COLOR(CYAN), stderr);
    atom_string = atom_to_string(atom);
    if (atom_string == NULL) {
        name_cookie = xcb_get_atom_name(connection, atom);
        name = xcb_get_atom_name_reply(connection, name_cookie, NULL);
        if (name == NULL) {
            fprintf(stderr, "%" PRIu32, atom);
        } else {
            fprintf(stderr, "%.*s", xcb_get_atom_name_name_length(name),
                    xcb_get_atom_name_name(name));
            free(name);
        }
        fputs(COLOR(RED) "<not known>", stderr);
    } else {
        fprintf(stderr, "%s", atom_string);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_visibility(xcb_visibility_t visibility)
{
    fputs(COLOR(CYAN), stderr);
    switch (visibility) {
    case XCB_VISIBILITY_UNOBSCURED:
        fputs("unobscured", stderr);
        break;
    case XCB_VISIBILITY_PARTIALLY_OBSCURED:
        fputs("partially obscured", stderr);
        break;
    case XCB_VISIBILITY_FULLY_OBSCURED:
        fputs("fully obscured", stderr);
        break;
    default:
        fprintf(stderr, "%u", visibility);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_configure_mask(uint32_t mask)
{
    static const char *flags[] = {
        "X", "Y", "WIDTH", "HEIGHT",
        "BORDER_WIDTH", "SIBLING", "STACK_MODE"
    };
    int flag_count = 0;

    fputs(COLOR(MAGENTA), stderr);
    for (const char **flag = flags; mask != 0; mask >>= 1, flag++) {
        if ((mask & 1)) {
            if (flag_count > 0) {
                fputs(CLEAR_COLOR "+" COLOR(MAGENTA), stderr);
            }
            fputs(*flag, stderr);
            flag_count++;
        }
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_mapping(xcb_mapping_t mapping)
{
    fputs(COLOR(CYAN), stderr);
    switch (mapping) {
    case XCB_MAPPING_MODIFIER:
        fputs("modifier", stderr);
        break;
    case XCB_MAPPING_KEYBOARD:
        fputs("keyboard", stderr);
        break;
    case XCB_MAPPING_POINTER:
        fputs("pointer", stderr);
        break;
    default:
        fprintf(stderr, "%u", mapping);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_wm_state(xcb_icccm_wm_state_t state)
{
    fputs(COLOR(CYAN), stderr);
    switch (state) {
    case XCB_ICCCM_WM_STATE_WITHDRAWN:
        fputs("withdrawn", stderr);
        break;
    case XCB_ICCCM_WM_STATE_NORMAL:
        fputs("normal", stderr);
        break;
    case XCB_ICCCM_WM_STATE_ICONIC:
        fputs("iconic", stderr);
        break;
    default:
        fprintf(stderr, "%u", state);
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_window_mode(window_mode_t mode)
{
    fputs(COLOR(CYAN), stderr);
    switch (mode) {
    case WINDOW_MODE_TILING:
        fputs("tiling", stderr);
        break;
    case WINDOW_MODE_FLOATING:
        fputs("floating", stderr);
        break;
    case WINDOW_MODE_DOCK:
        fputs("dock", stderr);
        break;
    case WINDOW_MODE_FULLSCREEN:
        fputs("fullscreen", stderr);
        break;
    case WINDOW_MODE_DESKTOP:
        fputs("desktop", stderr);
        break;
    case WINDOW_MODE_MAX:
        fputs("none", stderr);
        break;
    }
    fputs(CLEAR_COLOR, stderr);
}

static void log_connection_error(int error)
{
    fputs(COLOR(RED), stderr);
    switch (error) {
    case XCB_CONN_ERROR:
        fputs("socket, pipe or other stream error", stderr);
        break;
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
        fputs("an extension is not supported", stderr);
        break;
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        fputs("insufficient memory", stderr);
        break;
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
        fputs("maximum request length exceeded", stderr);
        break;
    case XCB_CONN_CLOSED_PARSE_ERR:
        fputs("failed parsing display string", stderr);
        break;
    case XCB_CONN_CLOSED_INVALID_SCREEN:
        fputs("no screen matching the display", stderr);
        break;
    case XCB_CONN_CLOSED_FDPASSING_FAILED:
        fputs("FD passing operation failed", stderr);
        break;
    default:
        fprintf(stderr, "%d", error);
    }
    fputs(CLEAR_COLOR, stderr);
}

/*************************/
/** Event log functions **/

#define V(string) fputs(", " string "=", stderr)

/* Log a RandrScreenChangeNotifyEvent to standard error output. */
static void log_randr_screen_change_notify_event(
        xcb_randr_screen_change_notify_event_t *event)
{
    V("size"); log_size(event->width, event->height);
    V("millimeter size"); log_size(event->mwidth, event->mheight);
}

/* Log an error to standard error output. */
static void log_generic_error(xcb_generic_error_t *error)
{
    const char *error_label;

    /* get textual representation of the error */
    error_label = xcb_event_get_error_label(error->error_code);
    if (error_label == NULL) {
        V("error_code"); log_unsigned(error->error_code);
    } else {
        V("error_label"); fprintf(stderr, COLOR(CYAN) "%s" CLEAR_COLOR,
                error_label);
    }
    V("code"); log_unsigned_pair(error->major_code, error->minor_code);
}

/* Log a KeyPress-/KeyReleaseEvent to standard error output. */
static void log_key_press_event(xcb_key_press_event_t *event)
{
    V("keycode"); log_unsigned(event->detail);
    V("time"); log_unsigned(event->time);
    V("root"); log_xcb_window(event->root);
    V("event"); log_xcb_window(event->event);
    V("child"); log_xcb_window(event->child);
    V("root_position"); log_point(event->root_x, event->root_y);
    V("event_position"); log_point(event->event_x, event->event_y);
    V("modifiers"); log_modifiers(event->state);
    V("same_screen"); log_boolean(event->same_screen);
}

/* Log a ButtonPress-/ButtonReleaseEvent to standard error output. */
static void log_button_press_event(xcb_button_press_event_t *event)
{
    V("button"); log_button(event->detail);
    V("time"); log_unsigned(event->time);
    V("root"); log_xcb_window(event->root);
    V("event"); log_xcb_window(event->event);
    V("child"); log_xcb_window(event->child);
    V("root_position"); log_point(event->root_x, event->root_y);
    V("event_position"); log_point(event->event_x, event->event_y);
    V("modifiers"); log_modifiers(event->state);
    V("same_screen"); log_boolean(event->same_screen);
}

/* Log a MotionNotifyEvent to standard error output. */
static void log_motion_notify_event(xcb_motion_notify_event_t *event)
{
    V("motion"); log_motion(event->detail);
    V("time"); log_unsigned(event->time);
    V("root"); log_xcb_window(event->root);
    V("event"); log_xcb_window(event->event);
    V("child"); log_xcb_window(event->child);
    V("root_position"); log_point(event->root_x, event->root_y);
    V("event_position"); log_point(event->event_x, event->event_y);
    V("modifiers"); log_modifiers(event->state);
    V("same_screen"); log_boolean(event->same_screen);
}

/* Log a Enter-/LeaveNotifyEvent to standard error output. */
static void log_enter_notify_event(xcb_enter_notify_event_t *event)
{
    V("detail"); log_notify_detail(event->detail);
    V("time"); log_unsigned(event->time);
    V("root"); log_xcb_window(event->root);
    V("event"); log_xcb_window(event->event);
    V("child"); log_xcb_window(event->child);
    V("root_position"); log_point(event->root_x, event->root_y);
    V("event_position"); log_point(event->event_x, event->event_y);
    V("modifiers"); log_modifiers(event->state);
    V("mode"); log_notify_mode(event->mode);
    V("same_screen_focus"); log_boolean(event->same_screen_focus);
}

/* Log a FocusIn-/FocusOutEvent to standard error output. */
static void log_focus_in_event(xcb_focus_in_event_t *event)
{
    V("detail"); log_notify_detail(event->detail);
    V("event"); log_xcb_window(event->event);
    V("mode"); log_notify_mode(event->mode);
}

/* Log a KeymapNotifyEvent to standard error output. */
static void log_keymap_notify_event(xcb_keymap_notify_event_t *event)
{
    (void) event;
    fprintf(stderr, ", keys=...");
}

/* Log a ExposeEvent to standard error output. */
static void log_expose_event(xcb_expose_event_t *event)
{
    V("window"); log_xcb_window(event->window);
    V("positon"); log_point(event->x, event->y);
    V("size"); log_size(event->width, event->height);
    V("count"); log_unsigned(event->count);
}

/* Log a GraphicsExposureEvent to standard error output. */
static void log_graphics_exposure_event(xcb_graphics_exposure_event_t *event)
{
    V("drawable"); log_hexadecimal(event->drawable);
    V("positon"); log_point(event->x, event->y);
    V("size"); log_size(event->width, event->height);
    V("count"); log_unsigned(event->count);
    V("opcode"); log_unsigned_pair(event->major_opcode, event->minor_opcode);
}

/* Log a NoExposureEvent to standard error output. */
static void log_no_exposure_event(xcb_no_exposure_event_t *event)
{
    V("drawable"); log_hexadecimal(event->drawable);
    V("opcode"); log_unsigned_pair(event->major_opcode, event->minor_opcode);
}

/* Log a VisibilityNotifyEvent to standard error output. */
static void log_visibility_notify_event(xcb_visibility_notify_event_t *event)
{
    V("window"); log_xcb_window(event->window);
    V("visibility"); log_visibility(event->state);
}

/* Log a CreateNotifyEvent to standard error output. */
static void log_create_notify_event(xcb_create_notify_event_t *event)
{
    V("parent"); log_xcb_window(event->parent);
    V("window"); log_xcb_window(event->window);
    V("position"); log_point(event->x, event->y);
    V("size"); log_size(event->width, event->height);
    V("border_width"); log_unsigned(event->border_width);
    V("override_redirect"); log_boolean(event->override_redirect);
}

/* Log a DestroyNotifyEvent to standard error output. */
static void log_destroy_notify_event(xcb_destroy_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
}

/* Log a UnmapNotifyEvent to standard error output. */
static void log_unmap_notify_event(xcb_unmap_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
    V("from_configure"); log_boolean(event->from_configure);
}

/* Log a MapNotifyEvent to standard error output. */
static void log_map_notify_event(xcb_map_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
    V("override_redirect"); log_boolean(event->override_redirect);
}

/* Log a MapRequestEvent to standard error output. */
static void log_map_request_event(xcb_map_request_event_t *event)
{
    V("parent"); log_xcb_window(event->parent);
    V("window"); log_xcb_window(event->window);
}

/* Log a ReparentNotifyEvent to standard error output. */
static void log_reparent_notify_event(xcb_reparent_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("parent"); log_xcb_window(event->parent);
    V("window"); log_xcb_window(event->window);
    V("position"); log_point(event->x, event->y);
    V("override_redirect"); log_boolean(event->override_redirect);
}

/* Log a ConfigureNotifyEvent to standard error output. */
static void log_configure_notify_event(xcb_configure_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
    V("above_sibling"); log_xcb_window(event->above_sibling);
    V("position"); log_point(event->x, event->y);
    V("size"); log_size(event->width, event->height);
    V("border_width"); log_unsigned(event->border_width);
    V("override_redirect"); log_boolean(event->override_redirect);
}

/* Log a ConfigureRequestEvent to standard error output. */
static void log_configure_request_event(xcb_configure_request_event_t *event)
{
    V("parent"); log_xcb_window(event->parent);
    V("window"); log_xcb_window(event->window);
    V("sibling"); log_xcb_window(event->sibling);
    V("position"); log_point(event->x, event->y);
    V("size"); log_size(event->width, event->height);
    V("border_width"); log_unsigned(event->border_width);
    V("value_mask"); log_configure_mask(event->value_mask);
}

/* Log a GravityNotifyEvent to standard error output. */
static void log_gravity_notify_event(xcb_gravity_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
    V("position"); log_point(event->x, event->y);
}

/* Log a ResizeRequestEvent to standard error output. */
static void log_resize_request_event(xcb_resize_request_event_t *event)
{
    V("window"); log_xcb_window(event->window);
    V("size"); log_size(event->width, event->height);
}

/* Log a CirculateNotify-/CirculateRequestEvent to standard error output. */
static void log_circulate_notify_event(xcb_circulate_notify_event_t *event)
{
    V("event"); log_xcb_window(event->event);
    V("window"); log_xcb_window(event->window);
    V("place"); log_place(event->place);
}

/* Log a PropertyNotifyEvent to standard error output. */
static void log_property_notify_event(xcb_property_notify_event_t *event)
{
    V("window"); log_xcb_window(event->window);
    V("atom"); log_atom(event->atom);
    V("time"); log_unsigned(event->time);
    V("state"); log_property_state(event->state);
}

/* Log a SelectionClearEvent to standard error output. */
static void log_selection_clear_event(xcb_selection_clear_event_t *event)
{
    V("time"); log_unsigned(event->time);
    V("owner"); log_unsigned(event->owner);
    V("selection"); log_atom(event->selection);
}

/* Log a SelectionRequestEvent to standard error output. */
static void log_selection_request_event(xcb_selection_request_event_t *event)
{
    V("time"); log_unsigned(event->time);
    V("owner"); log_unsigned(event->owner);
    V("requestor"); log_unsigned(event->requestor);
    V("selection"); log_atom(event->selection);
    V("target"); log_atom(event->target);
    V("property"); log_atom(event->property);
}

/* Log a SelectionNotifyEvent to standard error output. */
static void log_selection_notify_event(xcb_selection_notify_event_t *event)
{
    V("time"); log_unsigned(event->time);
    V("requestor"); log_unsigned(event->requestor);
    V("selection"); log_atom(event->selection);
    V("target"); log_atom(event->target);
    V("property"); log_atom(event->property);
}

/* Log a ColormapNotifyEvent to standard error output. */
static void log_colormap_notify_event(xcb_colormap_notify_event_t *event)
{
    V("window"); log_xcb_window(event->window);
    V("colormap"); log_hexadecimal(event->colormap);
    V("state"); log_unsigned(event->state);
}

/* Log a ClientMessageEvent to standard error output. */
static void log_client_message_event(xcb_client_message_event_t *event)
{
    const char *state_strings[] = {
        [_NET_WM_STATE_REMOVE] = "remove",
        [_NET_WM_STATE_ADD] = "add",
        [_NET_WM_STATE_TOGGLE] = "toggle",
    };

    V("format"); log_unsigned(event->format);
    V("window"); log_xcb_window(event->window);
    V("type"); log_atom(event->type);
    if (event->type == ATOM(_NET_CLOSE_WINDOW)) {
        V("time"); log_unsigned(event->data.data32[0]);
        V("source"); log_source(event->data.data32[1]);
    } else if (event->type == ATOM(_NET_MOVERESIZE_WINDOW)) {
        V("gravity"); log_gravity((event->data.data32[0] & 0xff));
        V("flags"); log_configure_mask(((event->data.data32[0] << 8) & 0x7));
        V("source"); log_source(event->data.data32[0] << 12);
        V("geometry"); log_rectangle(event->data.data32[1],
                event->data.data32[2], event->data.data32[3],
                event->data.data32[4]);
    } else if (event->type == ATOM(_NET_WM_MOVERESIZE)) {
        V("root_position"); log_point(event->data.data32[0],
                event->data.data32[1]);
        V("direction"); log_direction(event->data.data32[2]);
        V("button"); log_button(event->data.data32[3]);
        V("source"); log_source(event->data.data32[4]);
    } else if (event->type == ATOM(_NET_RESTACK_WINDOW)) {
        V("source"); log_source(event->data.data32[0]);
        V("sibling"); log_xcb_window(event->data.data32[1]);
        V("detail"); log_notify_detail(event->data.data32[2]);
    } else if (event->type == ATOM(_NET_REQUEST_FRAME_EXTENTS)) {
        /* no data */
    } else if (event->type == ATOM(_NET_WM_DESKTOP)) {
        V("new_desktop"); log_unsigned(event->data.data32[0]);
        V("source"); log_source(event->data.data32[1]);
    } else if (event->type == ATOM(_NET_ACTIVE_WINDOW)) {
        V("source"); log_unsigned(event->data.data32[0]);
        V("window"); log_xcb_window(event->data.data32[1]);
    } else if (event->type == ATOM(WM_CHANGE_STATE)) {
        V("state"); log_wm_state(event->data.data32[0]);
    } else if (event->type == ATOM(_NET_WM_STATE)) {
        V("data");
        if (event->data.data32[0] >= SIZE(state_strings)) {
            fprintf(stderr, COLOR(RED) "<misformatted>");
        } else {
            fprintf(stderr, COLOR(CYAN) "%s",
                    state_strings[event->data.data32[0]]);
        }
        fputc(' ', stderr);
        log_atom(event->data.data32[1]);
    } else {
        V("data32");
        fprintf(stderr, COLOR(GREEN) "%" PRIu32 " %" PRIu32 " %" PRIu32
                    " %" PRIu32 " %" PRIu32 CLEAR_COLOR,
                event->data.data32[0], event->data.data32[1],
                event->data.data32[2], event->data.data32[3],
                event->data.data32[4]);
    }
}

/* Log a MappingNotifyEvent to standard error output. */
static void log_mapping_notify_event(xcb_mapping_notify_event_t *event)
{
    V("request"); log_mapping(event->request);
    V("first_keycode"); log_unsigned(event->first_keycode);
    V("count"); log_unsigned(event->count);
}

/* Log a GeGenericEvent to standard error output. */
static void log_ge_generic_event(xcb_ge_generic_event_t *event)
{
    V("length"); log_unsigned(event->length);
    V("event_type"); log_unsigned(event->event_type);
    V("full_sequence"); log_unsigned(event->full_sequence);
}

/* Log an event to standard error output. */
static void log_event(xcb_generic_event_t *event)
{
    uint8_t event_type;

    event_type = (event->response_type & ~0x80);
    if (randr_event_base > 0 &&
            event_type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        fputs("RandrScreenChangeNotify", stderr);
    } else if (randr_event_base > 0 &&
            event_type == randr_event_base + XCB_RANDR_NOTIFY) {
        fputs("RandrNotify", stderr);
    } else if (xcb_event_get_label(event_type) != NULL) {
        fputs(xcb_event_get_label(event_type), stderr);
    } else {
        fprintf(stderr, "EVENT[%" PRIu8 "]", event_type);
    }

    if (event_type == XCB_GE_GENERIC) {
        xcb_ge_generic_event_t *const generic_event =
            (xcb_ge_generic_event_t*) event;
        if (generic_event->extension >= SIZE(generic_event_strings)) {
            fprintf(stderr, "EVENT[%" PRIu8 "]", event_type);
        } else {
            fprintf(stderr, "%s",
                    generic_event_strings[generic_event->extension]);
        }
    }

    fprintf(stderr, "(sequence=");
    log_unsigned(event->sequence);

    if (randr_event_base > 0 &&
            event_type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        log_randr_screen_change_notify_event(
                (xcb_randr_screen_change_notify_event_t*) event);
        fputs(")", stderr);
        return;
    }

    switch (event_type) {
    case 0:
        log_generic_error((xcb_generic_error_t*) event);
        break;

    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        log_key_press_event((xcb_key_press_event_t*) event);
        break;

    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        log_button_press_event((xcb_button_press_event_t*) event);
        break;

    case XCB_MOTION_NOTIFY:
        log_motion_notify_event((xcb_motion_notify_event_t*) event);
        break;

    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        log_enter_notify_event((xcb_enter_notify_event_t*) event);
        break;

    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        log_focus_in_event((xcb_focus_in_event_t*) event);
        break;

    case XCB_KEYMAP_NOTIFY:
        log_keymap_notify_event((xcb_keymap_notify_event_t*) event);
        break;

    case XCB_EXPOSE:
        log_expose_event((xcb_expose_event_t*) event);
        break;

    case XCB_GRAPHICS_EXPOSURE:
        log_graphics_exposure_event((xcb_graphics_exposure_event_t*) event);
        break;

    case XCB_NO_EXPOSURE:
        log_no_exposure_event((xcb_no_exposure_event_t*) event);
        break;

    case XCB_VISIBILITY_NOTIFY:
        log_visibility_notify_event((xcb_visibility_notify_event_t*) event);
        break;

    case XCB_CREATE_NOTIFY:
        log_create_notify_event((xcb_create_notify_event_t*) event);
        break;

    case XCB_DESTROY_NOTIFY:
        log_destroy_notify_event((xcb_destroy_notify_event_t*) event);
        break;

    case XCB_UNMAP_NOTIFY:
        log_unmap_notify_event((xcb_unmap_notify_event_t*) event);
        break;

    case XCB_MAP_NOTIFY:
        log_map_notify_event((xcb_map_notify_event_t*) event);
        break;

    case XCB_MAP_REQUEST:
        log_map_request_event((xcb_map_request_event_t*) event);
        break;

    case XCB_REPARENT_NOTIFY:
        log_reparent_notify_event((xcb_reparent_notify_event_t*) event);
        break;

    case XCB_CONFIGURE_NOTIFY:
        log_configure_notify_event((xcb_configure_notify_event_t*) event);
        break;

    case XCB_CONFIGURE_REQUEST:
        log_configure_request_event((xcb_configure_request_event_t*) event);
        break;

    case XCB_GRAVITY_NOTIFY:
        log_gravity_notify_event((xcb_gravity_notify_event_t*) event);
        break;

    case XCB_RESIZE_REQUEST:
        log_resize_request_event((xcb_resize_request_event_t*) event);
        break;

    case XCB_CIRCULATE_NOTIFY:
    case XCB_CIRCULATE_REQUEST:
        log_circulate_notify_event((xcb_circulate_notify_event_t*) event);
        break;

    case XCB_PROPERTY_NOTIFY:
        log_property_notify_event((xcb_property_notify_event_t*) event);
        break;

    case XCB_SELECTION_CLEAR:
        log_selection_clear_event((xcb_selection_clear_event_t*) event);
        break;

    case XCB_SELECTION_REQUEST:
        log_selection_request_event((xcb_selection_request_event_t*) event);
        break;

    case XCB_SELECTION_NOTIFY:
        log_selection_notify_event((xcb_selection_notify_event_t*) event);
        break;

    case XCB_COLORMAP_NOTIFY:
        log_colormap_notify_event((xcb_colormap_notify_event_t*) event);
        break;

    case XCB_CLIENT_MESSAGE:
        log_client_message_event((xcb_client_message_event_t*) event);
        break;

    case XCB_MAPPING_NOTIFY:
        log_mapping_notify_event((xcb_mapping_notify_event_t*) event);
        break;

    case XCB_GE_GENERIC:
        log_ge_generic_event((xcb_ge_generic_event_t*) event);
        break;
    }
    fputs(")", stderr);
}

/* Log an xcb error to standard error output. */
static void log_error(xcb_generic_error_t *error)
{
    fprintf(stderr, "(sequence=");
    log_integer(error->sequence);
    log_generic_error(error);
    fputs(")", stderr);
}

/* Log a window to standard error output. */
static void log_window(const Window *window)
{
    log_hexadecimal(window->client.id);
    fprintf(stderr, COLOR(YELLOW) "<%" PRIu32 ">" CLEAR_COLOR, window->number);
}

/* Log a window to standard error output. */
static void log_frame(const Frame *frame)
{
    fputs(COLOR(MAGENTA) "[", stderr);
    log_rectangle(frame->x, frame->y, frame->width, frame->height);
    fputs(COLOR(MAGENTA) "]" CLEAR_COLOR, stderr);
    if (frame->number > 0) {
        fprintf(stderr, COLOR(YELLOW) "<%" PRIu32 ">" CLEAR_COLOR,
                frame->number);
    }
}

/* Log a data type to standard error output. */
static void log_data_type(data_type_t type, const GenericData *data)
{
    switch (type) {
    case DATA_TYPE_VOID:
        /* nothing */
        break;

    case DATA_TYPE_BOOLEAN:
        fputc(' ', stderr);
        log_boolean(data->boolean);
        break;

    case DATA_TYPE_STRING:
        fputc(' ', stderr);
        fprintf(stderr, COLOR(GREEN) "%s" CLEAR_COLOR,
                (char*) data->string);
        break;

    case DATA_TYPE_INTEGER:
        fputc(' ', stderr);
        log_integer(data->integer);
        break;

    case DATA_TYPE_QUAD:
        fprintf(stderr, COLOR(GREEN)
                    " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
                    CLEAR_COLOR,
                data->quad[0], data->quad[1],
                data->quad[2], data->quad[3]);
        break;

    case DATA_TYPE_COLOR:
        fprintf(stderr, COLOR(YELLOW) " #%06" PRIx32 CLEAR_COLOR,
            data->color);
        break;

    case DATA_TYPE_MODIFIERS:
        fputc(' ', stderr);
        log_modifiers(data->modifiers);
        break;

    case DATA_TYPE_CURSOR:
        fprintf(stderr, " " COLOR(GREEN) "%s" CLEAR_COLOR,
                xcursor_core_strings[data->cursor]);
        break;

    /* not a real data type */
    case DATA_TYPE_MAX:
        break;
    }
}

/* Log an action to standard error output. */
static void log_actions(const Action *actions, uint32_t number_of_actions)
{
    for (uint32_t i = 0; i < number_of_actions; i++) {
        if (i > 0) {
            fputs(" ; ", stderr);
        }
        fprintf(stderr, COLOR(CYAN) "%s" CLEAR_COLOR,
                action_to_string(actions[i].code));
        log_data_type(get_action_data_type(actions[i].code), &actions[i].data);
    }
}

/* Log the screen information to standard error output. */
static void log_screen(xcb_screen_t *screen)
{
    fprintf(stderr, "Screen(root="); log_hexadecimal(screen->root);
    V("default_colormap"); log_hexadecimal(screen->default_colormap);
    V("white_pixel"); log_hexadecimal(screen->white_pixel);
    V("black_pixel"); log_hexadecimal(screen->black_pixel);
    V("size"); log_size(screen->width_in_pixels, screen->height_in_pixels);
    V("millimeter_size"); log_size(screen->width_in_millimeters,
                                screen->height_in_millimeters);
    fputs(")", stderr);
}

/* Print a formatted string to standard error output. */
void log_formatted(log_severity_t severity, const char *file, int line,
        const char *format, ...)
{
    va_list list;
    char buffer[64];
    time_t current_time;
    struct tm *tm;

    /* omit logging if not severe enough */
    if (log_severity > severity) {
        return;
    }

    /* print the time and file with line number at the front */
    current_time = time(NULL);
    tm = localtime(&current_time);
    strftime(buffer, sizeof(buffer),
            severity == LOG_SEVERITY_ERROR ? COLOR(RED) "{%F %T}" :
                COLOR(GREEN) "[%F %T]", tm);
    fprintf(stderr, "%s" COLOR(YELLOW) "(%s:%d) " CLEAR_COLOR,
            buffer, file, line);

    /* parse the format string */
    va_start(list, format);
    for (; format[0] != '\0'; format++) {
        if (format[0] == '%') {
            switch (format[1]) {
            case '%':
                format++;
                /* fall through */
            case '\0':
                fputc('%', stderr);
                continue;

            /* print a point */
            case 'P': {
                const int32_t x = va_arg(list, int32_t);
                log_point(x, va_arg(list, int32_t));
                break;
            }

            /* print a size */
            case 'S': {
                const uint32_t width = va_arg(list, uint32_t);
                log_size(width, va_arg(list, uint32_t));
                break;
            }

            /* print a rectangle */
            case 'R': {
                const int32_t x = va_arg(list, int32_t);
                const int32_t y = va_arg(list, int32_t);
                const uint32_t width = va_arg(list, uint32_t);
                log_rectangle(x, y, width, va_arg(list, uint32_t));
                break;
            }

            /* print a boolean */
            case 'b':
                log_boolean(va_arg(list, int));
                break;

            /* print an xcb window */
            case 'w':
                log_xcb_window(va_arg(list, xcb_window_t));
                break;

            /* print a window */
            case 'W':
                log_window(va_arg(list, Window*));
                break;

            /* print a window mode */
            case 'm':
                log_window_mode(va_arg(list, window_mode_t));
                break;

            /* print a frame */
            case 'F':
                log_frame(va_arg(list, Frame*));
                break;

            /* print an xcb event */
            case 'V':
                log_event(va_arg(list, xcb_generic_event_t*));
                break;

            /* print a list of actions */
            case 'A': {
                const uint32_t number_of_actions = va_arg(list, uint32_t);
                log_actions(va_arg(list, Action*), number_of_actions);
                break;
            }

            /* print an xcb atom */
            case 'a':
                log_atom(va_arg(list, xcb_atom_t));
                break;

            /* print an xcb connection error */
            case 'X':
                log_connection_error(va_arg(list, int));
                break;

            /* print an xcb error */
            case 'E':
                log_error(va_arg(list, xcb_generic_error_t*));
                break;

            /* print a screen */
            case 'C':
                log_screen(va_arg(list, xcb_screen_t*));
                break;

            /* standard print format specifiers */
            default: {
                uint32_t i = 0;

                fputs(COLOR(GREEN), stderr);

                /* move a segment of `format` into `buffer` and feed it into the
                 * real `printf()`
                 */
                do {
                    buffer[i] = format[i];
                    if (strchr(PRINTF_FORMAT_SPECIFIERS, format[i]) != NULL) {
                        buffer[i + 1] = '\0';
                        vfprintf(stderr, buffer, list);
                        fputs(CLEAR_COLOR, stderr);
                        format += i - 1;
                        /* all clean */
                        i = 0;
                        break;
                    }
                    i++;
                } while (format[i] != '\0' && i < sizeof(buffer) - 2);

                /* if anything is weird print the rest of the format string */
                if (i > 0) {
                    fputs(format, stderr);
                    while (format[2] != '\0') {
                        format++;
                    }
                }
                break;
            }
            }
            format++;
        } else {
            fputc(format[0], stderr);
        }
    }
    va_end(list);
}
