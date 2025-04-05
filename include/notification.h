#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "font.h" // XftDraw
#include "utility.h" // utf8_t
#include "x11_management.h" // XClient

/* notification window */
extern struct notification {
    /* the X correspondence */
    XClient client;
    /* Xft drawing context */
    XftDraw *xft_draw;
} Notification;

/* Show the notification window with given message at given coordinates for
 * a duration in seconds specified in the configuration.
 *
 * @message UTF-8 string.
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const utf8_t *message, int x, int y);

#endif
