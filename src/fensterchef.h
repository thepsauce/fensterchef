#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <xcb/render.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include <stdio.h>

#include "action.h"
#include "frame.h"

/* flag used to configure window position, size and border width */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT)

/* padding used for internal popup windows */
#define WINDOW_PADDING 6

/* duration of the notification window in seconds */
#define NOTIFICATION_DURATION 3

/* cast to FcChar8* which represents a utf8 string */
#define UTF8_TEXT(text) ((FcChar8*) (text))

/* get the screen at given index */
#define SCREEN(no) (g_ewmh.screens[no])

/* xcb server connection */
extern xcb_connection_t         *g_dpy;

/* ewmh information */
extern xcb_ewmh_connection_t    g_ewmh;

/* the screen being used */
extern uint32_t                 g_screen_no;

/* 1 while the window manager is running */
extern unsigned                 g_running;

/* stock object indexes */
enum {
    STOCK_WHITE_PEN,
    STOCK_BLACK_PEN,
    STOCK_GC,
    STOCK_INVERTED_GC,

    STOCK_COUNT,
};

/* graphis objects with the id referring to the xcb id */
extern uint32_t                 g_stock_objects[STOCK_COUNT];

/* general purpose values */
extern uint32_t                 g_values[6];

/* user notification window */
extern xcb_window_t             g_notification_window;

/* list of windows */
extern xcb_window_t             g_window_list_window;

/* Initialize most of fensterchef data and set root window flags. */
int init_fensterchef(void);

/* Quit the window manager and clean up resources. */
void quit_fensterchef(int exit_code);

/* Shows a windows list where the user can select a window from.
 * -- Implemented in window_list.c --
 */
Window *select_window_from_list(void);

/* Show the notification window with given message at given coordinates for
 * NOTIFICATION_DURATION seconds.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const FcChar8 *msg, int32_t x, int32_t y);

/* -- Implemented in event.c -- */

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event);

/* -- Implemented in render_font.c -- */

/* Creates a pixmap with width and height set to 1. */
xcb_pixmap_t create_pen(xcb_render_color_t color);

/* Initializes all parts needed for drawing fonts.
 *
 * @return 0 on success, 1 otherwise.
 */
int init_font_drawing(void);

/* Frees all resources associated to font rendering. */
void deinit_font_drawing(void);

/* This sets the globally used font for rendering.
 *
 * @name can be NULL to set the default font.
 *
 * @return 0 on success or 1 when the font was not found.
 */
int set_font(const FcChar8 *query);

/* This draws text to a given drawable using the current font. */
void draw_text(xcb_drawable_t xcb_drawable, const FcChar8 *utf8, uint32_t len,
        xcb_render_color_t background_color, xcb_rectangle_t *rect,
        xcb_render_picture_t foreground, int32_t x, int32_t y);

/* Measure a text that has no new lines. */
struct text_measure {
    int32_t ascent;
    int32_t descent;
    uint32_t total_width;
};

void measure_text(const FcChar8 *utf8, uint32_t len, struct text_measure *tm);

/* -- Implemented in keyboard.c -- */

/* Get a keysym from a keycode. */
xcb_keysym_t get_keysym(xcb_keycode_t keycode);

/* Get a list of keycodes from a keysym.
 * The caller needs to free this list.
 */
xcb_keycode_t *get_keycodes(xcb_keysym_t keysym);

/* Grab all keys so we receive the keypress events for them. */
int setup_keys(void);

/* Get an action code from a key press event (keybind).
 *
 * @return the action index or ACTION_NULL if the bind does not exist.
 */
action_t get_action_bind(xcb_key_press_event_t *event);

#endif
