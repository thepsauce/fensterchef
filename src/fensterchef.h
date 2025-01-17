#ifndef FENSTERCHEF_H
#define FENSTERCHEF_H

#include <fontconfig/fontconfig.h>
#include <freetype2/freetype/freetype.h>

#include <xcb/xcb.h>
#include <xcb/render.h>

#include <stdio.h>

#include "frame.h"

/* flag used to configure window position, size and border width */
#define XCB_CONFIG_SIZE (XCB_CONFIG_WINDOW_X | \
                         XCB_CONFIG_WINDOW_Y | \
                         XCB_CONFIG_WINDOW_WIDTH | \
                         XCB_CONFIG_WINDOW_HEIGHT | \
                         XCB_CONFIG_WINDOW_BORDER_WIDTH)

/* padding used for internal popup windows */
#define WINDOW_PADDING 6

#define UTF8_TEXT(text) ((FcChar8*) (text))

/* xcb server connection */
extern xcb_connection_t     *g_dpy;

/* screens */
extern xcb_screen_t         **g_screens;
extern uint32_t             g_screen_count;
/* the screen being used */
extern uint32_t             g_screen_no;

/* 1 while the window manager is running */
extern int                  g_running;

enum {
    STOCK_WHITE_PEN,
    STOCK_BLACK_PEN,
    STOCK_GC,
    STOCK_INVERTED_GC,
    STOCK_COUNT,
};

/* graphis objects with the id referring to the xcb id */
extern uint32_t             g_stock_objects[STOCK_COUNT];

/* the currently used font information */
extern struct font {
    /* freetype library */
    FT_Library library;
    /* the font face used for rendering */
    FT_Face face;
    /* the glyphset containing cached glyphs */
    xcb_render_glyphset_t glyphs;
}                           g_font;

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

/* Quit the window manager and clean up resources. */
void quit_fensterchef(int exit_code);

/* Shows a windows list where the user can select a window from.
 * -- Implemented in window_list.c --
 */
Window *select_window_from_list(void);

/* Show the notification window with given message at given coordinates.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const FcChar8 *msg, int32_t x, int32_t y);

/* Handle the mapping of a new window. */
void accept_new_window(xcb_window_t xcb_window);

/* Handle the given xcb event. */
void handle_event(xcb_generic_event_t *event);

/* -- Implemented in render_font.c -- */

/* Creates a pixmap with width and height 1. */
xcb_pixmap_t create_pen(xcb_render_color_t color);

/* This sets the globally used font for rendering.
 *
 * @name can be NULL to set the default font.
 *
 * @return 1 when the font was not found, 0 otherwise.
 */
int set_font(const FcChar8 *query);

/* This draws text to a given drawable using the global font. */
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

/* Handle a key press event. */
void handle_key_press(xcb_key_press_event_t *event);

#endif
