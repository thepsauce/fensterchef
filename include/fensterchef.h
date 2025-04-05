#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <stdbool.h>

#define FENSTERCHEF_NAME "fensterchef"

#define FENSTERCHEF_VERSION "1.3"

#define FENSTERCHEF_CONFIGURATION "fensterchef/fensterchef.config"

/* the home directory */
extern const char *Fensterchef_home;

/* the path of the configuration file */
extern char *Fensterchef_configuration;

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

/* Output the frames and windows into a file as textual output.
 *
 * @return ERROR if the file could not be opened, OK otherwise.
 */
int dump_frames_and_windows(const char *file_path);

/* Close the display to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code);

#endif
