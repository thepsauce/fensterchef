#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <stdbool.h>
#include <stdio.h>

#include "utf8.h"

#define FENSTERCHEF_NAME "fensterchef"

#define FENSTERCHEF_VERSION "1.3"

#define FENSTERCHEF_CONFIGURATION "~/.config/fensterchef/fensterchef.config"

/* the home directory */
extern const char *Fensterchef_home;

/* the path of the configuration file */
extern const char *Fensterchef_configuration;

/* true while the window manager is running */
extern bool Fensterchef_is_running;

/* Spawn a window that has the `FENSTERCHEF_COMMAND` property.
 *
 * This should be called before any initialization has be done.
 *
 * This requests to close fensterchef by setting `is_fensterchef_running` to
 * false.
 */
void run_external_command(const char *command);

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code);

/* Show the notification window with given message at given coordinates for
 * a duration in seconds specified in the configuration.
 *
 * @message UTF-8 string.
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const utf8_t *message, int32_t x, int32_t y);

#endif
