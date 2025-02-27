#ifndef RENDER_H
#define RENDER_H

/* this setups freetype macros including FT_FREETYPE_H which is the normal
 * include path for freetype
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <xcb/xcb.h>
#include <xcb/render.h>

#include "utf8.h"

/* stock object indexes */
enum {
    /* xcb_render_picture_t used for drawing text */
    STOCK_WHITE_PEN,
    /* xcb_render_picture_t used for drawing text */
    STOCK_BLACK_PEN,

    /* xcb_gcontext_t used for drawing shapes */
    STOCK_GC,
    /* xcb_gcontext_t used for drawing shapes (inverted colors) */
    STOCK_INVERTED_GC,

    /* not a real stock object */
    STOCK_MAX,
};

/* graphical objects with the id referring to the X id */
extern uint32_t stock_objects[STOCK_MAX];

/* Initialize the graphical stock objects that can be used for rendering. */
int initialize_renderer(void);

/* Frees all resources associated to rendering. */
void deinitialize_renderer(void);

/* Creates a picture for the given window (or retrieves it from the cache). */
xcb_render_picture_t cache_window_picture(xcb_drawable_t xcb_drawable);

/* Set the color of a pen. */
void set_pen_color(xcb_render_picture_t pen, xcb_render_color_t color);

/* Creates a picture with width and height set to 1. */
xcb_render_picture_t create_pen(xcb_render_color_t color);

/* Helper function to convert an RRGGBB color into an xcb color. */
static inline void convert_color_to_xcb_color(xcb_render_color_t *xcb_color,
        uint32_t color)
{
    /* color values go from 0x0000 (lowest intensity) to
     * 0xff00 (maximum intensity)
     */
    xcb_color->alpha = 0xff00;
    xcb_color->red = (color & 0xff0000) >> 8;
    xcb_color->green = (color & 0xff00);
    xcb_color->blue = (color & 0xff) << 8;
}

/* This sets the globally used font for rendering.
 *
 * @return OK on success or ERROR when the font was not found.
 */
int set_font(const utf8_t *query);

/* This draws text to a given drawable using the current font. */
int draw_text(xcb_drawable_t xcb_drawable, const utf8_t *utf8, uint32_t length,
        xcb_render_color_t background_color, const xcb_rectangle_t *rectangle,
        xcb_render_picture_t foreground, int32_t x, int32_t y);

/* Measure a text that has no new lines. */
struct text_measure {
    int32_t ascent;
    int32_t descent;
    uint32_t total_width;
};

void measure_text(const utf8_t *utf8, uint32_t len, struct text_measure *tm);

#endif
