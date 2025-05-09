#ifndef CONFIGURATION_H
#define CONFIGURATION_H

/**
 * The fensterchef configuration management.
 *
 * The default configuration of fensterchef is included within its binary.
 */

#include "action.h"
#include "utility/types.h"
#include "utility/utility.h"

/* the currently loaded configuration settings */
extern struct configuration {
    /* how many pixels off the edges of windows should be used for resizing */
    int resize_tolerance;

    /* the number the first window gets assigned */
    unsigned first_window_number;

    /* at which percentage to count windows to be overlapped with a monitor */
    unsigned overlap;

    /* whether to automatically create a split when a window is shown */
    bool auto_split;
    /* whether to automatically equalize all frames within the root */
    bool auto_equalize;
    /* whether to fill in empty frames automatically */
    bool auto_fill_void;
    /* whether to remove frames automatically when their inner windows is
     * hidden
     */
    bool auto_remove;
    /* whether to remove frames automatically when they become empty */
    bool auto_remove_void;

    /* the duration in seconds a notification window should linger for */
    unsigned notification_duration;

    /* padding of text within the notification window */
    unsigned text_padding;

    /* width of the border */
    unsigned border_size;
    /* color of the border around the window */
    uint32_t border_color;
    /* color of the border of an unfocused tiling/floating windows */
    uint32_t border_color_active;
    /* color of the border of a focused window */
    uint32_t border_color_focus;
    /* color of the text */
    uint32_t foreground;
    /* color of the background of fensterchef windows */
    uint32_t background;

    /* width of the inner gaps (between frames) */
    int gaps_inner[4];
    /* width of the outer gaps (between frames and monitor boundaries */
    int gaps_outer[4];
} configuration;

/* the settings of the default configuration */
extern const struct configuration default_configuration;

#define DEFAULT_CONFIGURATION_MERGE_SETTINGS        (1 << 0)
#define DEFAULT_CONFIGURATION_MERGE_KEY_BINDINGS    (1 << 1)
#define DEFAULT_CONFIGURATION_MERGE_BUTTON_BINDINGS (1 << 2)
#define DEFAULT_CONFIGURATION_MERGE_FONT            (1 << 3)
#define DEFAULT_CONFIGURATION_MERGE_CURSOR          (1 << 4)
#define DEFAULT_CONFIGURATION_MERGE_ALL            ((1 << 5) - 1)

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags);

/* Get the configuration file path for fensterchef.
 *
 * Do not use the returned pointer after a second call to this function.
 */
const char *get_configuration_file(void);

/* Clear everything that is currently loaded in the configuration. */
void clear_configuration(void);

/* Reload the fensterchef configuration.
 *
 * This either loads the configuration or the default configuration if that
 * failed.
 */
void reload_configuration(void);

#endif
