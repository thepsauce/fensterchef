#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>

#include "fensterchef.h"
#include "log.h"
#include "resources.h"
#include "x11_management.h"

/* the cached resources we are interested in */
struct resources resources;

/* Set a resource. */
void set_resource(const char *class, const char *name, const char *value,
        bool is_loose)
{
    /* this is noot needed because it is already implied by `class[0] == '\0'`
     * implied! not equivalent!
     */
    (void) is_loose;

    /* handle:
     * Xcursor.theme <string>
     * Xcursor.size <integer>
     * Xcursor.anim <boolean>
     * Xcursor.resized <boolean>
     * Xcursor.theme_core <boolean>
     */
    if (strcmp(class, "Xcursor") == 0 || class[0] == '\0') {
        if (strcmp(name, "theme") == 0) {
            free(xcursor_settings.theme);
            xcursor_settings.theme = xstrdup(value);
        } else if (strcmp(name, "size") == 0) {
            xcursor_settings.size = strtol(value, NULL, 10);
        } else if (strcmp(name, "anim") == 0) {
            xcursor_settings.animated = xcursor_string_to_boolean(value);
        } else if (strcmp(name, "resized") == 0) {
            xcursor_settings.resized = xcursor_string_to_boolean(value);
        } else if (strcmp(name, "theme_core") == 0) {
            xcursor_settings.theme_core = xcursor_string_to_boolean(value);
        }
    }

    /* handle:
     * Xft.dpi <integer>
     */
    if (strcmp(class, "Xft") == 0 || class[0] == '\0') {
        if (strcmp(name, "dpi") == 0) {
            resources.dpi = strtoul(value, NULL, 10);
        }
    }
}

static void parse_resources(char *string)
{
    char *class, *name, *value;
    char *separator;
    bool is_loose;

    /* the general format is:
     * `<class>.<name>: <value>`
     */
    while (string[0] != '\0') {
        while (isspace(string[0])) {
            string++;
        }

        /* <class> */
        class = string;

        /* find the separating ':' */
        separator = strchr(string, ':');
        if (separator == NULL) {
            LOG("RESOURCE_MANAGER is misformatted\n");
            break;
        }

        /* <name> */
        name = separator;
        /* allow some tolerance for no particular reason */
        while (isspace(name[-1])) {
            name--;
        }
        name[0] = '\0';
        while (name[-1] != '.' && name[-1] != '*') {
            name--;
        }

        /* accept all but `.<name>` */
        if (name[-1] == '.' && class[0] == '\0') {
            LOG("RESOURCE_MANAGER is misformatted\n");
            break;
        }

        is_loose = name[-1] == '*';
        name[-1] = '\0';

        /* <value> */
        value = separator + 1;
        while (isspace(value[0])) {
            value++;
        }

        separator = strchr(value, '\n');
        string = separator + 1;

        /* null terminate the value */
        if (separator != NULL) {
            while (isspace(separator[-1])) {
                separator--;
            }
            separator[0] = '\0';
        }

        set_resource(class, name, value, is_loose);

        if (separator == NULL) {
            break;
        }
    }
}

/* Load the X resources from the root window's `RESOURCE_MANAGER` atom. */
void reload_resources(void)
{
    xcb_get_property_cookie_t resource_cookie;
    xcb_get_property_reply_t *resource;

    set_default_xcursor_settings();

    resources.dpi = 0;

    resource_cookie = xcb_get_property(connection, false, screen->root,
            XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0,
            16 * 1024 /* this should be big enough */);
    resource = xcb_get_property_reply(connection, resource_cookie, NULL);
    if (resource != NULL) {
        /* need to create a copy because it is not null-terminated which is very
         * inconvenient
         */
        char *const string = xstrndup(xcb_get_property_value(resource),
                xcb_get_property_value_length(resource));
        free(resource);
        parse_resources(string);
        free(string);
    }

    overwrite_xcursor_settings();

    /* if no dpi was given, make it a sensible value */
    if (resources.dpi == 0) {
        /* compute the dpi (dots per inch) with the pixel size and millimeter
         * size, multiplying by 254 and then diving by 10 converts the
         * millimeters to inches and adding half the divisor before dividing
         * will round better
         */
        resources.dpi = (screen->width_in_pixels * 254 +
                screen->width_in_millimeters * 5) /
            (screen->width_in_millimeters * 10);
    }

    /* when the theme changed for example, we do not want to let old cursors
     * appear and instead use the ones from the new theme
     */
    clear_cursor_cache();
}
