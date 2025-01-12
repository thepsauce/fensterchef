#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <xcb/xcb.h>

#include <stdio.h>

#ifdef DEBUG
#define LOG(fp, fmt, ...) fprintf((fp), (fmt), ##__VA_ARGS__)
#else
#define LOG(fp, fmt, ...)
#endif

/* xcb server connection */
extern xcb_connection_t     *g_dpy;

/* screens */
extern xcb_screen_t         **g_screens;
extern uint32_t             g_screen_count;
/* the screen being used */
extern uint32_t             g_screen_no;

/* 1 while the window manager is running */
extern int                  g_running;

/* general purpose values */
extern uint32_t             g_values[5];

/* init the connection to xcb */
void init_connection(void);

/* log the screens information to a file */
void log_screens(FILE *fp);

/* initialize the screens */
void init_screens(void);

/* subscribe to event substructe redirecting so that we receive map/unmap
 * requests */
int take_control(void);

/* handle the mapping of a new window */
void accept_new_window(xcb_drawable_t win);

/* handle the next event xcb has */
void handle_event(void);

/* close the connection to the xcb server */
void close_connection(void);

/* implemented in keyboard.c */

/* grab all keys so we receive the keypress events for them */
int setup_keys(void);

/* handle a key press event */
void handle_key_press(xcb_key_press_event_t *ev);

#endif
