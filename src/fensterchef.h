#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <xcb/xcb.h>

#include <stdio.h>

#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT | \
                         XCB_CONFIG_WINDOW_BORDER_WIDTH)

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

/* Initialize the screens. */
void init_screens(void);

/* Initialize most of fensterchef data and set root window flags. */
int init_fensterchef(void);

/* Show the notification window with given message at given coordinates.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const char *msg, int32_t x, int32_t y);

/* Handle the mapping of a new window. */
void accept_new_window(xcb_drawable_t xcb_window);

/* Handle the next event xcb has. */
void handle_next_event(void);

/* implemented in keyboard.c */

/* Grab all keys so we receive the keypress events for them. */
int setup_keys(void);

/* Handle a key press event. */
void handle_key_press(xcb_key_press_event_t *event);

#endif
