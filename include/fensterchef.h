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
#include "window.h"

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

/* Show the notification window with given message at given coordinates for
 * NOTIFICATION_DURATION seconds.
 *
 * @x Center x position.
 * @y Center y position.
 */
void set_notification(const FcChar8 *msg, int32_t x, int32_t y);

#endif
