#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <xcb/xcb.h>

#include <stdio.h>

#include "frame.h"

/* flag used to configure window position, size and border width */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT | \
                         XCB_CONFIG_WINDOW_BORDER_WIDTH)

/* padding used for internal popup windows */
#define WINDOW_PADDING 2

/* xcb server connection */
extern xcb_connection_t     *g_dpy;

/* screens */
extern xcb_screen_t         **g_screens;
extern uint32_t             g_screen_count;
/* the screen being used */
extern uint32_t             g_screen_no;

/* 1 while the window manager is running */
extern int                  g_running;

/* the graphics context used for drawing black text on white background */
extern xcb_gcontext_t       g_drawing_context;

/* same as g_drawing_context but with inverted colors */
extern xcb_gcontext_t       g_inverted_context;

/* the font used for rendering */
extern xcb_font_t           g_font;

/* general purpose values */
extern uint32_t             g_values[6];

enum {
    WM_PROTOCOLS,
    WM_DELETE_WINDOW,

    FIRST_NET_ATOM,
    NET_SUPPORTED = FIRST_NET_ATOM,
    NET_FULLSCREEN,
    NET_WM_STATE,
    NET_ACTIVE,

    ATOM_COUNT,
};

/* string names of the atoms */
extern const char           *g_atom_names[ATOM_COUNT];

/* all interned atoms */
extern xcb_atom_t           g_atoms[ATOM_COUNT];

/* user notification window */
extern xcb_window_t         g_notification_window;

/* list of windows */
extern xcb_window_t         g_window_list_window;

/* Initialize the screens. */
void init_screens(void);

/* Initialize most of fensterchef data and set root window flags. */
int init_fensterchef(void);

/* Shows a windows list where the user can select a window from.
 * -- Implemented in window_list.c --
 */
Window *select_window_from_list(void);

/* Show the notification window with given message at given coordinates.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const char *msg, int32_t x, int32_t y);

/* Handle the mapping of a new window. */
void accept_new_window(xcb_window_t xcb_window);

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event);

/* -- Implemented in keyboard.c -- */

/* Get a keysym from a keycode. */
xcb_keysym_t get_keysym(xcb_keycode_t keycode);

/* Get a list of keycodes from a keysym.
 * The caller needs to free this list.
 */
xcb_keycode_t *get_keycodes(xcb_keysym_t keysym);

/* Grab all keys so we receive the keypress events for them. */
int setup_keys(void);

/* Handle a key press event. */
void handle_key_press(xcb_key_press_event_t *event);

#endif
