#ifndef NOTIFY_H
#define NOTIFY_H

#include "fensterchef.h"

/* The notification window. */
extern xcb_window_t g_notification_window;

/* Initialize the notification window.
 */
int init_notification(void);

/* Show the notification window with given message at given coordinates.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const char *msg, int32_t x, int32_t y);

#endif
