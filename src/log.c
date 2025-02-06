#ifdef DEBUG

#include <inttypes.h>

#include "fensterchef.h"
#include "log.h"
#include "screen.h"
#include "util.h"

/* the file to log to */
FILE *g_log_file;

/* Log key modifiers to the log file. */
static void log_modifiers(uint32_t mask)
{
    static const char *modifiers[] = {
        "Shift", "Lock", "Ctrl", "Alt",
        "Mod2", "Mod3", "Mod4", "Mod5",
        "Button1", "Button2", "Button3", "Button4", "Button5"
    };
    int mod_count = 0;

    for (const char **modifier = modifiers; mask != 0; mask >>= 1,
            modifier++) {
        if ((mask & 1)) {
            if (mod_count > 0) {
                fprintf(g_log_file, "+");
            }
            fprintf(g_log_file, *modifier);
            mod_count++;
        }
    }
}

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

/* Log an event to the log file. */
void log_event(xcb_generic_event_t *event)
{
    uint8_t                         event_type;
    xcb_generic_error_t             *error;
    xcb_key_press_event_t           *kp;
    xcb_button_press_event_t        *bp;
    xcb_motion_notify_event_t       *mn;
    xcb_enter_notify_event_t        *en;
    xcb_focus_in_event_t            *fi;
    xcb_keymap_notify_event_t       *kn;
    xcb_expose_event_t              *e;
    xcb_graphics_exposure_event_t   *ge;
    xcb_no_exposure_event_t         *ne;
    xcb_visibility_notify_event_t   *vn;
    xcb_create_notify_event_t       *crn;
    xcb_destroy_notify_event_t      *dn;
    xcb_unmap_notify_event_t        *un;
    xcb_map_notify_event_t          *man;
    xcb_map_request_event_t         *mr;
    xcb_reparent_notify_event_t     *rn;
    xcb_configure_notify_event_t    *cn;
    xcb_configure_request_event_t   *cr;
    xcb_gravity_notify_event_t      *gn;
    xcb_resize_request_event_t      *rr;
    xcb_circulate_notify_event_t    *cin;
    xcb_circulate_request_event_t   *cir;
    xcb_property_notify_event_t     *pn;
    xcb_selection_clear_event_t     *sc;
    xcb_selection_request_event_t   *sr;
    xcb_selection_notify_event_t    *sn;
    xcb_colormap_notify_event_t     *con;
    xcb_client_message_event_t      *cln;
    xcb_mapping_notify_event_t      *min;
    xcb_ge_generic_event_t          *gen;

    LOG("");
    event_type = (event->response_type & ~0x80);
    fputs(xcb_event_get_label(event_type), g_log_file);
    if (event_type == XCB_GE_GENERIC) {
        gen = (xcb_ge_generic_event_t*) event;
        if (gen->extension >= SIZE(generic_event_strings)) {
            fprintf(g_log_file, "UNKNOWN_EVENT[%" PRIu8 "]", event_type);
        } else {
            fprintf(g_log_file, "%s", generic_event_strings[gen->extension]);
        }
    }

    fputc('(', g_log_file);
    switch (event_type) {
    case 0:
        error = (xcb_generic_error_t*) event;
        fprintf(g_log_file, "label=%s, code=%" PRIu8 ", sequence=%" PRIu16 ", major=%" PRIu8 ", minor=%" PRIu16,
                xcb_event_get_error_label(error->error_code),
                error->error_code, error->sequence, error->major_code,
                error->minor_code);
        break;

    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        kp = (xcb_key_press_event_t*) event;
        fprintf(g_log_file, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", mod=",
                kp->time, kp->root, kp->event, kp->child, kp->detail);
        log_modifiers(kp->state);
        break;

    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        bp = (xcb_button_press_event_t*) event;
        fprintf(g_log_file, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", mod=",
                bp->time, bp->root, bp->event, bp->child, bp->detail);
        log_modifiers(bp->state);
        break;

    case XCB_MOTION_NOTIFY:
        mn = (xcb_motion_notify_event_t*) event;
        fprintf(g_log_file, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", root_x=%" PRId16 ", root_y=%" PRId16 ", event_x=%" PRId16 ", event_y=%" PRId16 ", mod=",
                mn->time, mn->root, mn->event, mn->child, mn->detail, mn->root_x, mn->root_y, mn->event_x, mn->event_y);
        log_modifiers(mn->state);
        break;

    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        en = (xcb_enter_notify_event_t*) event;
        fprintf(g_log_file, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", root_x=%" PRId16 ", root_y=%" PRId16 ", event_x=%" PRId16 ", event_y=%" PRId16,
                en->time, en->root, en->event, en->child, en->root_x, en->root_y, en->event_x, en->event_y);
        break;

    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        fi = (xcb_focus_in_event_t*) event;
        fprintf(g_log_file, "mode=%" PRIu8 ", detail=%" PRIu8 ", event=%" PRIu32, fi->mode, fi->detail, fi->event);
        break;

    case XCB_KEYMAP_NOTIFY:
        kn = (xcb_keymap_notify_event_t*) event;
        (void) kn;
        fprintf(g_log_file, "keys=..."); // Keymap is a large array; implement if needed
        break;

    case XCB_EXPOSE:
        e = (xcb_expose_event_t*) event;
        fprintf(g_log_file, "x=%" PRIu16 ", y=%" PRIu16 ", width=%" PRIu16 ", height=%" PRIu16 ", count=%" PRIu16,
                e->x, e->y, e->width, e->height, e->count);
        break;

    case XCB_GRAPHICS_EXPOSURE:
        ge = (xcb_graphics_exposure_event_t*) event;
        fprintf(g_log_file, "x=%" PRIu16 ", y=%" PRIu16 ", width=%" PRIu16 ", height=%" PRIu16 ", count=%" PRIu16 ", minor_opcode=%" PRIu8 ", major_opcode=%" PRIu8,
                ge->x, ge->y, ge->width, ge->height, ge->count, ge->minor_opcode, ge->major_opcode);
        break;

    case XCB_NO_EXPOSURE:
        ne = (xcb_no_exposure_event_t*) event;
        fprintf(g_log_file, "sequence=%" PRIu16 ", minor_opcode=%" PRIu8 ", major_opcode=%" PRIu8,
                ne->sequence, ne->minor_opcode, ne->major_opcode);
        break;

    case XCB_VISIBILITY_NOTIFY:
        vn = (xcb_visibility_notify_event_t*) event;
        fprintf(g_log_file, "state=%" PRIu8, vn->state);
        break;

    case XCB_CREATE_NOTIFY:
        crn = (xcb_create_notify_event_t*) event;
        fprintf(g_log_file, "parent=%" PRIu32 ", window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                crn->parent, crn->window, crn->x, crn->y, crn->width, crn->height);
        break;

    case XCB_DESTROY_NOTIFY:
        dn = (xcb_destroy_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32, dn->window);
        break;

    case XCB_UNMAP_NOTIFY:
        un = (xcb_unmap_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", from_configure=%" PRIu8,
                un->window, un->from_configure);
        break;

    case XCB_MAP_NOTIFY:
        man = (xcb_map_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", override_redirect=%" PRIu8,
                man->window, man->override_redirect);
        break;

    case XCB_MAP_REQUEST:
        mr = (xcb_map_request_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", parent=%" PRIu32,
                mr->window, mr->parent);
        break;

    case XCB_REPARENT_NOTIFY:
        rn = (xcb_reparent_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", parent=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16,
                rn->window, rn->parent, rn->x, rn->y);
        break;

    case XCB_CONFIGURE_NOTIFY:
        cn = (xcb_configure_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                cn->window, cn->x, cn->y, cn->width, cn->height);
        break;

    case XCB_CONFIGURE_REQUEST:
        cr = (xcb_configure_request_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                cr->window, cr->x, cr->y, cr->width, cr->height);
        break;

    case XCB_GRAVITY_NOTIFY:
        gn = (xcb_gravity_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16,
                gn->window, gn->x, gn->y);
        break;

    case XCB_RESIZE_REQUEST:
        rr = (xcb_resize_request_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", width=%" PRIu16 ", height=%" PRIu16,
                rr->window, rr->width, rr->height);
        break;

    case XCB_CIRCULATE_NOTIFY:
        cin = (xcb_circulate_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", place=%" PRIu8,
                cin->window, cin->place);
        break;

    case XCB_CIRCULATE_REQUEST:
        cir = (xcb_circulate_request_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", place=%" PRIu8,
                cir->window, cir->place);
        break;

    case XCB_PROPERTY_NOTIFY:
        pn = (xcb_property_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", atom=%" PRIu32 ", time=%" PRIu32 ", state=%" PRIu8,
                pn->window, pn->atom, pn->time, pn->state);
        break;

    case XCB_SELECTION_CLEAR:
        sc = (xcb_selection_clear_event_t*) event;
        fprintf(g_log_file, "owner=%" PRIu32 ", selection=%" PRIu32 ", time=%" PRIu32,
                sc->owner, sc->selection, sc->time);
        break;

    case XCB_SELECTION_REQUEST:
        sr = (xcb_selection_request_event_t*) event;
        fprintf(g_log_file, "owner=%" PRIu32 ", requestor=%" PRIu32 ", selection=%" PRIu32 ", target=%" PRIu32,
                sr->owner, sr->requestor, sr->selection, sr->target);
        break;

    case XCB_SELECTION_NOTIFY:
        sn = (xcb_selection_notify_event_t*) event;
        fprintf(g_log_file, "requestor=%" PRIu32 ", selection=%" PRIu32 ", target=%" PRIu32 ", property=%" PRIu32 ", time=%" PRIu32,
                sn->requestor, sn->selection, sn->target, sn->property, sn->time);
        break;

    case XCB_COLORMAP_NOTIFY:
        con = (xcb_colormap_notify_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", colormap=%" PRIu32 ", new=%" PRIu8 ", state=%" PRIu8,
                con->window, con->colormap, con->_new, con->state);
        break;

    case XCB_CLIENT_MESSAGE:
        cln = (xcb_client_message_event_t*) event;
        fprintf(g_log_file, "window=%" PRIu32 ", type=%" PRIu32
                ", data=%" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
                cln->window, cln->type, cln->data.data32[0], cln->data.data32[1],
                cln->data.data32[2], cln->data.data32[3], cln->data.data32[4]);
        break;

    case XCB_MAPPING_NOTIFY:
        min = (xcb_mapping_notify_event_t*) event;
        fprintf(g_log_file, "request=%" PRIu8 ", first_keycode=%" PRIu8 ", count=%" PRIu8,
                min->request, min->first_keycode, min->count);
        break;

    case XCB_GE_GENERIC:
        gen = (xcb_ge_generic_event_t*) event;
        fprintf(g_log_file, "sequence=%" PRIu16 ", length=%" PRIu32,
                gen->sequence, gen->length);
        break;
    }
    fputs(")\n", g_log_file);
}

/* Log an xcb error to the log file with additional output formatting and new
 * line.
 */
void log_error(xcb_generic_error_t *error, const char *fmt, ...)
{
    va_list list;
    const char *error_label;

    ERR("");

    va_start(list, fmt);
    vfprintf(g_log_file, fmt, list);
    va_end(list);

    error_label = xcb_event_get_error_label(error->error_code);
    if (error_label != NULL) {
        fprintf(g_log_file, "X11 error: %s", error_label);
    } else {
        fprintf(g_log_file, "Unknown X11 error: ");
    }
    fprintf(g_log_file, "(code=%d, sequence=%d, major=%d, minor=%d)\n",
            error->error_code, error->sequence, error->major_code,
            error->minor_code);
}

/* Log the screen information to a file. */
void log_screen(void)
{
    LOG("Screen with root %u(width=%u, height=%u, white_pixel=%u, black_pixel=%u)\n",
            g_screen->xcb_screen->root,
            g_screen->xcb_screen->width_in_pixels,
            g_screen->xcb_screen->height_in_pixels,
            g_screen->xcb_screen->white_pixel,
            g_screen->xcb_screen->black_pixel);
}

#else

/* prevent compiler error */
typedef int i_am_not_empty;

#endif
