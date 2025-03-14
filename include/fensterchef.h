#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <stdbool.h>
#include <stdio.h>

#include "utf8.h"

#define FENSTERCHEF_NAME "fensterchef"

#define FENSTERCHEF_VERSION "1.1"

#define FENSTERCHEF_CONFIGURATION "~/.config/fensterchef/fensterchef.config"

/* the path of the configuration file */
extern const char *fensterchef_configuration;

/* true while the window manager is running */
extern bool is_fensterchef_running;

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code);

/* Show the notification window with given message at given coordinates for
 * a duration in seconds specified in the configuration.
 *
 * @message UTF-8 string.
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const uint8_t *message, int32_t x, int32_t y);

#endif
