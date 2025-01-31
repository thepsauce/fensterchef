#ifndef RENDER_FONT_H
#define RENDER_FONT_H

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <xcb/xcb.h>

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

/* Creates a pixmap with width and height set to 1. */
xcb_pixmap_t create_pen(xcb_window_t root, xcb_render_color_t color);

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

#endif
