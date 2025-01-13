#ifdef DEBUG

#include <inttypes.h>
#include <stdio.h>
#include <xcb/xcb_event.h>

#include "fensterchef.h"
#include "log.h"
#include "util.h"

static void log_modifiers(uint32_t mask, FILE *fp)
{
    const char *modifiers[] = {
            "Shift", "Lock", "Ctrl", "Alt",
            "Mod2", "Mod3", "Mod4", "Mod5",
            "Button1", "Button2", "Button3", "Button4", "Button5"
    };
    int mod_count = 0;

    for (const char **modifier = modifiers; mask != 0; mask >>= 1,
            modifier++) {
        if ((mask & 1)) {
            if (mod_count > 0) {
                fprintf(fp, "+");
            }
            fprintf(fp, *modifier);
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

/* Log an event to a file.  */
void log_event(xcb_generic_event_t *ev, FILE *fp)
{
    uint8_t                         ev_type;
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

    ev_type = (ev->response_type & ~0x80);
    fputs(xcb_event_get_label(ev_type), fp);
    if (ev_type == XCB_GE_GENERIC) {
        gen = (xcb_ge_generic_event_t*) ev;
        if (gen->extension >= SIZE(generic_event_strings)) {
            fprintf(fp, "[WRONG_EVENT(%" PRIu8 ")]", ev_type);
        } else {
            fprintf(fp, "[%s]", generic_event_strings[gen->extension]);
        }
    }
    fputc('(', fp);
    switch (ev_type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        kp = (xcb_key_press_event_t*) ev;
        fprintf(fp, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", mod=",
                kp->time, kp->root, kp->event, kp->child, kp->detail);
        log_modifiers(kp->state, fp);
        break;

    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        bp = (xcb_button_press_event_t*) ev;
        fprintf(fp, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", mod=",
                bp->time, bp->root, bp->event, bp->child, bp->detail);
        log_modifiers(bp->state, fp);
        break;

    case XCB_MOTION_NOTIFY:
        mn = (xcb_motion_notify_event_t*) ev;
        fprintf(fp, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", detail=%" PRIu8 ", root_x=%" PRId16 ", root_y=%" PRId16 ", event_x=%" PRId16 ", event_y=%" PRId16 ", mod=",
                mn->time, mn->root, mn->event, mn->child, mn->detail, mn->root_x, mn->root_y, mn->event_x, mn->event_y);
        log_modifiers(mn->state, fp);
        break;

    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        en = (xcb_enter_notify_event_t*) ev;
        fprintf(fp, "time=%" PRIu32 ", root=%" PRIu32 ", event=%" PRIu32 ", child=%" PRIu32 ", root_x=%" PRId16 ", root_y=%" PRId16 ", event_x=%" PRId16 ", event_y=%" PRId16,
                en->time, en->root, en->event, en->child, en->root_x, en->root_y, en->event_x, en->event_y);
        break;

    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        fi = (xcb_focus_in_event_t*) ev;
        fprintf(fp, "mode=%" PRIu8 ", detail=%" PRIu8, fi->mode, fi->detail);
        break;

    case XCB_KEYMAP_NOTIFY:
        kn = (xcb_keymap_notify_event_t*) ev;
        (void) kn;
        fprintf(fp, "keys=..."); // Keymap is a large array; implement if needed
        break;

    case XCB_EXPOSE:
        e = (xcb_expose_event_t*) ev;
        fprintf(fp, "x=%" PRIu16 ", y=%" PRIu16 ", width=%" PRIu16 ", height=%" PRIu16 ", count=%" PRIu16,
                e->x, e->y, e->width, e->height, e->count);
        break;

    case XCB_GRAPHICS_EXPOSURE:
        ge = (xcb_graphics_exposure_event_t*) ev;
        fprintf(fp, "x=%" PRIu16 ", y=%" PRIu16 ", width=%" PRIu16 ", height=%" PRIu16 ", count=%" PRIu16 ", minor_opcode=%" PRIu8 ", major_opcode=%" PRIu8,
                ge->x, ge->y, ge->width, ge->height, ge->count, ge->minor_opcode, ge->major_opcode);
        break;

    case XCB_NO_EXPOSURE:
        ne = (xcb_no_exposure_event_t*) ev;
        fprintf(fp, "sequence=%" PRIu16 ", minor_opcode=%" PRIu8 ", major_opcode=%" PRIu8,
                ne->sequence, ne->minor_opcode, ne->major_opcode);
        break;

    case XCB_VISIBILITY_NOTIFY:
        vn = (xcb_visibility_notify_event_t*) ev;
        fprintf(fp, "state=%" PRIu8, vn->state);
        break;

    case XCB_CREATE_NOTIFY:
        crn = (xcb_create_notify_event_t*) ev;
        fprintf(fp, "parent=%" PRIu32 ", window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                crn->parent, crn->window, crn->x, crn->y, crn->width, crn->height);
        break;

    case XCB_DESTROY_NOTIFY:
        dn = (xcb_destroy_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32, dn->window);
        break;

    case XCB_UNMAP_NOTIFY:
        un = (xcb_unmap_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", from_configure=%" PRIu8,
                un->window, un->from_configure);
        break;

    case XCB_MAP_NOTIFY:
        man = (xcb_map_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", override_redirect=%" PRIu8,
                man->window, man->override_redirect);
        break;

    case XCB_MAP_REQUEST:
        mr = (xcb_map_request_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", parent=%" PRIu32,
                mr->window, mr->parent);
        break;

    case XCB_REPARENT_NOTIFY:
        rn = (xcb_reparent_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", parent=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16,
                rn->window, rn->parent, rn->x, rn->y);
        break;

    case XCB_CONFIGURE_NOTIFY:
        cn = (xcb_configure_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                cn->window, cn->x, cn->y, cn->width, cn->height);
        break;

    case XCB_CONFIGURE_REQUEST:
        cr = (xcb_configure_request_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16 ", width=%" PRIu16 ", height=%" PRIu16,
                cr->window, cr->x, cr->y, cr->width, cr->height);
        break;

    case XCB_GRAVITY_NOTIFY:
        gn = (xcb_gravity_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", x=%" PRIi16 ", y=%" PRIi16,
                gn->window, gn->x, gn->y);
        break;

    case XCB_RESIZE_REQUEST:
        rr = (xcb_resize_request_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", width=%" PRIu16 ", height=%" PRIu16,
                rr->window, rr->width, rr->height);
        break;

    case XCB_CIRCULATE_NOTIFY:
        cin = (xcb_circulate_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", place=%" PRIu8,
                cin->window, cin->place);
        break;

    case XCB_CIRCULATE_REQUEST:
        cir = (xcb_circulate_request_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", place=%" PRIu8,
                cir->window, cir->place);
        break;

    case XCB_PROPERTY_NOTIFY:
        pn = (xcb_property_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", atom=%" PRIu32 ", time=%" PRIu32 ", state=%" PRIu8,
                pn->window, pn->atom, pn->time, pn->state);
        break;

    case XCB_SELECTION_CLEAR:
        sc = (xcb_selection_clear_event_t*) ev;
        fprintf(fp, "owner=%" PRIu32 ", selection=%" PRIu32 ", time=%" PRIu32,
                sc->owner, sc->selection, sc->time);
        break;

    case XCB_SELECTION_REQUEST:
        sr = (xcb_selection_request_event_t*) ev;
        fprintf(fp, "owner=%" PRIu32 ", requestor=%" PRIu32 ", selection=%" PRIu32 ", target=%" PRIu32,
                sr->owner, sr->requestor, sr->selection, sr->target);
        break;

    case XCB_SELECTION_NOTIFY:
        sn = (xcb_selection_notify_event_t*) ev;
        fprintf(fp, "requestor=%" PRIu32 ", selection=%" PRIu32 ", target=%" PRIu32 ", property=%" PRIu32 ", time=%" PRIu32,
                sn->requestor, sn->selection, sn->target, sn->property, sn->time);
        break;

    case XCB_COLORMAP_NOTIFY:
        con = (xcb_colormap_notify_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", colormap=%" PRIu32 ", new=%" PRIu8 ", state=%" PRIu8,
                con->window, con->colormap, con->_new, con->state);
        break;

    case XCB_CLIENT_MESSAGE:
        cln = (xcb_client_message_event_t*) ev;
        fprintf(fp, "window=%" PRIu32 ", type=%" PRIu32
                ", data=%" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
                cln->window, cln->type, cln->data.data32[0], cln->data.data32[1],
                cln->data.data32[2], cln->data.data32[3], cln->data.data32[4]);
        break;

    case XCB_MAPPING_NOTIFY:
        min = (xcb_mapping_notify_event_t*) ev;
        fprintf(fp, "request=%" PRIu8 ", first_keycode=%" PRIu8 ", count=%" PRIu8,
                min->request, min->first_keycode, min->count);
        break;

    case XCB_GE_GENERIC:
        fprintf(fp, "sequence=%" PRIu16 ", length=%" PRIu32,
                gen->sequence, gen->length);
        break;
    }
    fputc(')', fp);
}

/* TODO: figure out what these are used for
XCB_CREATE_WINDOW 1
XCB_CHANGE_WINDOW_ATTRIBUTES 2
XCB_GET_WINDOW_ATTRIBUTES 3
XCB_DESTROY_WINDOW 4
XCB_DESTROY_SUBWINDOWS 5
XCB_CHANGE_SAVE_SET 6
XCB_REPARENT_WINDOW 7
XCB_MAP_WINDOW 8
XCB_MAP_SUBWINDOWS 9
XCB_UNMAP_WINDOW 10
XCB_UNMAP_SUBWINDOWS 11
XCB_CONFIGURE_WINDOW 12
XCB_CIRCULATE_WINDOW 13
XCB_GET_GEOMETRY 14
XCB_QUERY_TREE 15
XCB_INTERN_ATOM 16
XCB_GET_ATOM_NAME 17
XCB_CHANGE_PROPERTY 18
XCB_DELETE_PROPERTY 19
XCB_GET_PROPERTY 20
XCB_LIST_PROPERTIES 21
XCB_SET_SELECTION_OWNER 22
XCB_GET_SELECTION_OWNER 23
XCB_CONVERT_SELECTION 24
XCB_SEND_EVENT 25
XCB_GRAB_POINTER 26
XCB_UNGRAB_POINTER 27
XCB_GRAB_BUTTON 28
XCB_UNGRAB_BUTTON 29
XCB_CHANGE_ACTIVE_POINTER_GRAB 30
XCB_GRAB_KEYBOARD 31
XCB_UNGRAB_KEYBOARD 32
XCB_GRAB_KEY 33
XCB_UNGRAB_KEY 34
XCB_ALLOW_EVENTS 35
XCB_GRAB_SERVER 36
XCB_UNGRAB_SERVER 37
XCB_QUERY_POINTER 38
XCB_GET_MOTION_EVENTS 39
XCB_TRANSLATE_COORDINATES 40
XCB_WARP_POINTER 41
XCB_SET_INPUT_FOCUS 42
XCB_GET_INPUT_FOCUS 43
XCB_QUERY_KEYMAP 44
XCB_OPEN_FONT 45
XCB_CLOSE_FONT 46
XCB_QUERY_FONT 47
XCB_QUERY_TEXT_EXTENTS 48
XCB_LIST_FONTS 49
XCB_LIST_FONTS_WITH_INFO 50
XCB_SET_FONT_PATH 51
XCB_GET_FONT_PATH 52
XCB_CREATE_PIXMAP 53
XCB_FREE_PIXMAP 54
XCB_CREATE_GC 55
XCB_CHANGE_GC 56
XCB_COPY_GC 57
XCB_SET_DASHES 58
XCB_SET_CLIP_RECTANGLES 59
XCB_FREE_GC 60
XCB_CLEAR_AREA 61
XCB_COPY_AREA 62
XCB_COPY_PLANE 63
XCB_POLY_POINT 64
XCB_POLY_LINE 65
XCB_POLY_SEGMENT 66
XCB_POLY_RECTANGLE 67
XCB_POLY_ARC 68
XCB_FILL_POLY 69
XCB_POLY_FILL_RECTANGLE 70
XCB_POLY_FILL_ARC 71
XCB_PUT_IMAGE 72
XCB_GET_IMAGE 73
XCB_POLY_TEXT_8 74
XCB_POLY_TEXT_16 75
XCB_IMAGE_TEXT_8 76
XCB_IMAGE_TEXT_16 77
XCB_CREATE_COLORMAP 78
XCB_FREE_COLORMAP 79
XCB_COPY_COLORMAP_AND_FREE 80
XCB_INSTALL_COLORMAP 81
XCB_UNINSTALL_COLORMAP 82
XCB_LIST_INSTALLED_COLORMAPS 83
XCB_ALLOC_COLOR 84
XCB_ALLOC_NAMED_COLOR 85
XCB_ALLOC_COLOR_CELLS 86
XCB_ALLOC_COLOR_PLANES 87
XCB_FREE_COLORS 88
XCB_STORE_COLORS 89
XCB_STORE_NAMED_COLOR 90
XCB_QUERY_COLORS 91
XCB_LOOKUP_COLOR 92
XCB_CREATE_CURSOR 93
XCB_CREATE_GLYPH_CURSOR 94
XCB_FREE_CURSOR 95
XCB_RECOLOR_CURSOR 96
XCB_QUERY_BEST_SIZE 97
XCB_QUERY_EXTENSION 98
XCB_LIST_EXTENSIONS 99
XCB_CHANGE_KEYBOARD_MAPPING 100
XCB_GET_KEYBOARD_MAPPING 101
XCB_CHANGE_KEYBOARD_CONTROL 102
XCB_GET_KEYBOARD_CONTROL 103
XCB_BELL 104
XCB_CHANGE_POINTER_CONTROL 105
XCB_GET_POINTER_CONTROL 106
XCB_SET_SCREEN_SAVER 107
XCB_GET_SCREEN_SAVER 108
XCB_CHANGE_HOSTS 109
XCB_LIST_HOSTS 110
XCB_SET_ACCESS_CONTROL 111
XCB_SET_CLOSE_DOWN_MODE 112
XCB_KILL_CLIENT 113
XCB_ROTATE_PROPERTIES 114
XCB_FORCE_SCREEN_SAVER 115
XCB_SET_POINTER_MAPPING 116
XCB_GET_POINTER_MAPPING 117
XCB_SET_MODIFIER_MAPPING 118
XCB_GET_MODIFIER_MAPPING 119
XCB_NO_OPERATION 127
*/

/* Log the screens information to a file. */
void log_screens(FILE *fp)
{
    xcb_screen_t            *screen;

    LOG(fp, "Have %u screen(s):\n", g_screen_count);
    for (uint32_t i = 0; i < g_screen_count; i++) {
        screen = g_screens[i];
        LOG(fp, "Screen %u ; %u:\n", i, screen->root);
        LOG(fp, "  width.........: %u\n", screen->width_in_pixels);
        LOG(fp, "  height........: %u\n", screen->height_in_pixels);
        LOG(fp, "  white pixel...: %u\n", screen->white_pixel);
        LOG(fp, "  black pixel...: %u\n", screen->black_pixel);
        LOG(fp, "\n");
    }
}

#else

/* prevent compiler error */
typedef int i_am_not_empty;

#endif
