#include <inttypes.h>

#include "fensterchef.h"
#include "window.h"

/* Update the short_name of the window. */
static void update_window_name(Window *window)
{
    xcb_get_property_cookie_t name_cookie;
    xcb_ewmh_get_utf8_strings_reply_t data;

    name_cookie = xcb_ewmh_get_wm_name(&g_ewmh, window->xcb_window);

    if (!xcb_ewmh_get_wm_name_reply(&g_ewmh, name_cookie, &data, NULL)) {
        snprintf((char*) window->properties.short_name,
                sizeof(window->properties.short_name),
                "%" PRId32 "_", window->number);
        return;
    }

    snprintf((char*) window->properties.short_name,
            sizeof(window->properties.short_name),
        "%" PRId32 "_%.*s",
            window->number,
            (int) MIN(data.strings_len, (uint32_t) INT_MAX), data.strings);

    xcb_ewmh_get_utf8_strings_reply_wipe(&data);
}

/* Update the size_hints of the window. */
static void update_window_size_hints(Window *window)
{
    xcb_get_property_cookie_t size_hints_cookie;

    size_hints_cookie = xcb_icccm_get_wm_size_hints(g_dpy, window->xcb_window,
            XCB_ATOM_WM_NORMAL_HINTS);
    if (!xcb_icccm_get_wm_size_hints_reply(g_dpy, size_hints_cookie,
                &window->properties.size_hints, NULL)) {
        window->properties.size_hints.flags = 0;
    }
}

/* Update the hints of the window. */
static void update_window_hints(Window *window)
{
    xcb_get_property_cookie_t hints_cookie;

    hints_cookie = xcb_icccm_get_wm_hints(g_dpy, window->xcb_window);
    if (!xcb_icccm_get_wm_hints_reply(g_dpy, hints_cookie,
                &window->properties.hints, NULL)) {
        window->properties.hints.flags = 0;
    }
}

/* Update the strut partial property of the window. */
static void update_window_strut(Window *window)
{
    xcb_get_property_cookie_t strut_partial_cookie;
    xcb_get_property_cookie_t strut_cookie;
    xcb_ewmh_get_extents_reply_t struts;

    strut_partial_cookie = xcb_ewmh_get_wm_strut_partial(&g_ewmh,
            window->xcb_window);
    if (xcb_ewmh_get_wm_strut_partial_reply(&g_ewmh, strut_partial_cookie,
                &window->properties.struts, NULL)) {
        window->properties.has_strut_partial = 1;
        return;
    }

    window->properties.has_strut_partial = 0;

    /* _NET_WM_STRUT is older than _NET_WM_STRUT_PARTIAL, fall back to it when
     * there is so strut partial
     */
    strut_cookie = xcb_ewmh_get_wm_strut(&g_ewmh, window->xcb_window);
    if (xcb_ewmh_get_wm_strut_reply(&g_ewmh, strut_cookie, &struts, NULL)) {
        window->properties.has_strut = 1;
        window->properties.struts.left = struts.left;
        window->properties.struts.top = struts.top;
        window->properties.struts.right = struts.right;
        window->properties.struts.bottom = struts.bottom;
    } else {
        window->properties.has_strut = 0;
    }
}

/* Update the transient property of the window. */
static void update_window_transient(Window *window)
{
    xcb_window_t transient;
    xcb_get_property_cookie_t transient_cookie;

    transient_cookie = xcb_icccm_get_wm_transient_for(g_dpy,
            window->xcb_window);
    if (xcb_icccm_get_wm_transient_for_reply(g_dpy, transient_cookie,
                &transient, NULL) && transient != 0) {
        window->properties.is_transient = 1;
    } else {
        window->properties.is_transient = 0;
    }
}

/* Update the fullscreen property of the window. */
static void update_window_fullscreen(Window *window)
{
    xcb_get_property_reply_t *state_reply;
    xcb_get_property_cookie_t state_cookie;
    xcb_atom_t *atoms;
    int num_atoms;

    state_cookie = xcb_get_property(g_dpy, 0, window->xcb_window,
            g_ewmh._NET_WM_STATE, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
    state_reply = xcb_get_property_reply(g_dpy, state_cookie, NULL);
    if (state_reply == NULL) {
        window->properties.is_fullscreen = 0;
        return;
    }

    if (state_reply->format != 0) {
        atoms = xcb_get_property_value(state_reply);
        num_atoms = xcb_get_property_value_length(state_reply);
        num_atoms /= state_reply->format / 8;
        for (int i = 0; i < num_atoms; i++) {
            if (atoms[i] == g_ewmh._NET_WM_STATE_FULLSCREEN) {
                window->properties.is_fullscreen = 1;
                break;
            }
        }
    }
    free(state_reply);
}

/* Update the given property of given window. */
void update_window_property(Window *window, window_property_t property)
{
    static void (*const updaters[])(Window *window) = {
        [WINDOW_PROPERTY_NAME] = update_window_name,
        [WINDOW_PROPERTY_SIZE_HINTS] = update_window_size_hints,
        [WINDOW_PROPERTY_HINTS] = update_window_hints,
        [WINDOW_PROPERTY_STRUT] = update_window_strut,
        [WINDOW_PROPERTY_FULLSCREEN] = update_window_fullscreen,
        [WINDOW_PROPERTY_TRANSIENT] = update_window_transient,
    };

    updaters[property](window);
}

/* Update all properties of given window. */
void update_all_window_properties(Window *window)
{
    for (window_property_t i = 0; i < WINDOW_PROPERTY_MAX; i++) {
        update_window_property(window, i);
    }
}
