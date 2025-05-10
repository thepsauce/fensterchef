#ifndef X11_SYNCHRONIZE_H
#define X11_SYNCHRONIZE_H

#include <X11/Xlib.h>

/**
 * Synchronization with the X11 server.  Keeping all data up to date.
 *
 * The essence of this window manager is to send to unneeded events to the X
 * server by having a fake live state intenally.  This is put live on the next
 * event cycle.
 */

#define SYNCHRONIZE_CLIENT_LIST     (1 << 0)
#define SYNCHRONIZE_ROOT_CURSOR     (1 << 1)
#define SYNCHRONIZE_BUTTON_BINDING  (1 << 2)
#define SYNCHRONIZE_KEY_BINDING     (1 << 3)
#define SYNCHRONIZE_ALL            ((1 << 4) - 1)

/* More information is synchronized without the use of flags which includes:
 * - Updating the window stack ordering
 */

/* hints set by all parts of the program indicating that a specific part needs
 * to be synchronized
 */
extern unsigned synchronization_flags;

/* first index of an xkb event/error */
extern int xkb_event_base, xkb_error_base;

/* this is the first index of a randr event/error */
extern int randr_event_base, randr_error_base;

/* display to the X server */
extern Display *display;

/* Open the X11 display (X server connection). */
void open_connection(void);

/* Synchronize the local data with the X server.
 *
 * @flags is an or combination of `SYNCHRONIZE_*` and is combined with
 *        `synchronization_flags` if it is 0.
 */
void synchronize_with_server(unsigned flags);

#endif
