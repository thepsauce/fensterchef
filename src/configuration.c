#include <errno.h>
#include <unistd.h>

#include "binding.h"
#include "bits/default_configuration.h"
#include "configuration.h"
#include "cursor.h"
#include "fensterchef.h"
#include "font.h"
#include "log.h"
#include "parse/data_type.h"
#include "parse/parse.h"
#include "parse/stream.h"
#include "window.h"

/* the currently loaded configuration settings */
struct configuration configuration;

/* Puts the button bindings of the default configuration into the current
 * configuration.
 */
static void merge_with_default_button_bindings(void)
{
    struct action_list_item item;
    struct button_binding binding;

    binding.actions.items = &item;
    item.data_count = 0;
    binding.actions.number_of_items = 1;
    binding.actions.data = NULL;

    /* overwrite bindings with the default button bindings */
    for (unsigned i = 0; i < SIZE(default_button_bindings); i++) {
        /* bake a button binding from the default button bindings struct */
        binding.is_transparent = false;
        binding.is_release = default_button_bindings[i].is_release;
        binding.modifiers = default_button_bindings[i].modifiers;
        binding.button = default_button_bindings[i].button_index;
        item.type = default_button_bindings[i].type;

        set_button_binding(&binding);
    }
}

/* Puts the key bindings of the default configuration into the current
 * configuration.
 */
static void merge_with_default_key_bindings(void)
{
    struct action_list_item item;
    struct parse_generic_data data;
    struct key_binding binding;

    binding.is_release = false;
    binding.key_code = 0;
    binding.actions.items = &item;
    item.data_count = 0;
    binding.actions.number_of_items = 1;
    binding.actions.data = &data;
    data.flags = 0;

    /* overwrite bindings with the default button bindings */
    for (unsigned i = 0; i < SIZE(default_key_bindings); i++) {
        /* bake a key binding from the bindings array */
        binding.modifiers = default_key_bindings[i].modifiers;
        binding.key_symbol = default_key_bindings[i].key_symbol;
        item.type = default_key_bindings[i].action;
        if (default_key_bindings[i].string != NULL) {
            item.data_count = 1;
            data.type = PARSE_DATA_TYPE_STRING;
            data.u.string = (utf8_t*) default_key_bindings[i].string;
        } else {
            item.data_count = 0;
        }

        set_key_binding(&binding);
    }
}

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags)
{
    if ((flags & DEFAULT_CONFIGURATION_MERGE_SETTINGS)) {
        memcpy(&configuration, &default_configuration, sizeof(configuration));
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_CURSOR)) {
        /* this makes it so the default cursors are loaded again on demand */
        clear_cursor_cache();
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_BUTTON_BINDINGS)) {
        merge_with_default_button_bindings();
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_KEY_BINDINGS)) {
        merge_with_default_key_bindings();
    }

    if ((flags & DEFAULT_CONFIGURATION_MERGE_FONT)) {
        set_font(default_font);
    }
}

/* Expand given @path.
 *
 * @path becomes invalid after this call, use the return value for the new
 * value.
 */
static char *expand_path(char *path)
{
    char *expanded;

    if (path[0] == '~' && path[1] == '/') {
        expanded = xasprintf("%s%s",
                Fensterchef_home, &path[1]);
        free(path);
    } else {
        expanded = path;
    }
    return expanded;
}

/* Expand the path and check if it is readable. */
static bool is_readable(char *path)
{
    if (access(path, R_OK) != 0) {
        if (errno != ENOENT) {
            LOG_ERROR("could not open %s: %s\n",
                    path, strerror(errno));
        }
        return false;
    }
    return true;
}

/* Get the configuration file for fensterchef. */
const char *get_configuration_file(void)
{
    static char *cached_path;

    const char *xdg_config_home, *xdg_config_dirs;
    char *path;
    const char *colon, *next;
    size_t length;

    if (Fensterchef_configuration != NULL &&
            is_readable(Fensterchef_configuration)) {
        return Fensterchef_configuration;
    }

    xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home == NULL) {
        xdg_config_home = "~/.config";
    }

    path = xasprintf("%s/" FENSTERCHEF_CONFIGURATION,
            xdg_config_home);
    path = expand_path(path);
    LOG_DEBUG("trying configuration path: %s\n",
            path);
    if (is_readable(path)) {
        free(cached_path);
        cached_path = path;
        return path;
    }
    free(path);

    xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
    if (xdg_config_dirs == NULL) {
        xdg_config_dirs = "/usr/local/share:/usr/share";
    }

    next = xdg_config_dirs;
    do {
        colon = strchr(next, ':');
        if (colon != NULL) {
            length = colon - next;
            colon++;
        } else {
            length = strlen(next);
        }

        path = xasprintf("%.*s/" FENSTERCHEF_CONFIGURATION,
                length, next);
        path = expand_path(path);
        LOG_DEBUG("trying configuration path: %s\n",
                path);
        if (is_readable(path)) {
            free(cached_path);
            cached_path = path;
            break;
        }
        free(path);
        path = NULL;

        next = colon;
    } while (next != NULL);

    return path;
}

/* Clear everything that is currently loaded in the configuration. */
void clear_configuration(void)
{
    clear_cursor_cache();
    clear_button_bindings();
    clear_key_bindings();
    clear_window_associations();
}

/* Reload the fensterchef configuration. */
void reload_configuration(void)
{
    InputStream *stream;

    const char *const configuration = get_configuration_file();

    if (configuration == NULL) {
        return;
    }

    clear_configuration();

    /* the top of the file always starts with these default modifiers */
    set_additional_modifiers(Mod4Mask);
    set_ignored_modifiers(LockMask | Mod2Mask);

    stream = create_file_stream(configuration);
    if (stream == NULL) {
        LOG("could not open %s: %s\n",
                configuration, strerror(errno));
        merge_default_configuration(DEFAULT_CONFIGURATION_MERGE_ALL);
    } else if (parse_stream_and_run_actions(stream) != OK) {
        merge_default_configuration(DEFAULT_CONFIGURATION_MERGE_ALL);
    }
    destroy_stream(stream);
}
