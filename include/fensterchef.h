#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <fontconfig/fontconfig.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/render.h>

#include <stdbool.h>
#include <stdio.h>

#define FENSTERCHEF_NAME "fensterchef"

#define FENSTERCHEF_CONFIGURATION ".config/fensterchef/fensterchef.config"

/* flag used to configure window position, size and border width */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT)

/* xcb server connection */
extern xcb_connection_t         *connection;

/* ewmh (extended window manager hints) information */
extern xcb_ewmh_connection_t    ewmh;

/* true while the window manager is running */
extern bool                     is_fensterchef_running;

/* general purpose values */
extern uint32_t                 general_values[7];

/* Initialize logging, the xcb/ewmh connection and font drawing. */
int initialize_fensterchef(int *screen_number);

/* Close the connection to xcb and exit the program with given exit code. */
void quit_fensterchef(int exit_code);

/* Show the notification window with given message at given coordinates for
 * NOTIFICATION_DURATION seconds.
 *
 * @message UTF-8 string.
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const uint8_t *message, int32_t x, int32_t y);

#endif
