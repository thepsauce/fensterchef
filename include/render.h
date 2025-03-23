#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>

/* this setups freetype macros including FT_FREETYPE_H which is the normal
 * include path for freetype
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <xcb/xcb.h>
#include <xcb/render.h>

#include "utf8.h"

/* the version of the underlying X renderer */
extern struct xrender_version {
    uint32_t major;
    uint32_t minor;
} Render_version;

/* Initialize the legacy font drawing. */
void initialize_legacy_font_drawing(void);

/* Initialize the modern font drawing. */
int initialize_modern_font_drawing(void);

/* Check if modern fonts are initialized. */
bool has_modern_font_drawing(void);

/* Initialize the graphical objects that can be used for rendering and font
 * drawing.
 *
 * @return OK even when the renderer completely failed initializing.
 *         This is so start up can continue. This is just for rendering the
 *         fensterchef windows anyway. The fensterchef core can function without
 *         it.
 */
int initialize_renderer(void);

/* Get the format of a visual. */
xcb_render_pictformat_t find_visual_format(xcb_visualid_t visual);

/* Get a good picture format for given depth. */
xcb_render_pictformat_t get_picture_format(uint8_t depth);

/* Helper function to convert an RRGGBB color into an xcb color. */
static inline void convert_color_to_xcb_color(xcb_render_color_t *xcb_color,
        uint32_t color)
{
    /* color values go from 0x0000 (lowest intensity) to
     * 0xff00 (maximum intensity)
     */
    xcb_color->alpha = 0xffff;
    xcb_color->red = (color & 0xff0000) >> 8;
    xcb_color->green = (color & 0xff00);
    xcb_color->blue = (color & 0xff) << 8;
}

/* Set the globally used font for rendering.
 *
 * @return OK on success or ERROR when the font was not found.
 */
int set_modern_font(const utf8_t *query);

/* text measurement */
struct text_measure {
    int32_t ascent;
    int32_t descent;
    uint32_t total_width;
    uint32_t total_height;
};

/* Measure the text using the core font. */
void measure_text_legacy(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure);

/* Measure the text using the modern font system. */
void measure_text_modern(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure);

/* Measure text using the current font.
 *
 * This chooses whether to use the legacy or modern font.
 */
void measure_text(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure);

/* Draw text using the X core font. */
void draw_text_legacy(xcb_drawable_t drawable, const utf8_t *utf8,
        uint32_t length, uint32_t background_color,
        const xcb_rectangle_t *rectangle, uint32_t foreground_color,
        int32_t x, int32_t y);

/* Draw text using the modern client side rendering. */
void draw_text_modern(xcb_drawable_t drawable, const utf8_t *utf8,
        uint32_t length, uint32_t background_color,
        const xcb_rectangle_t *rectangle, uint32_t foreground_color,
        int32_t x, int32_t y);

/* Draw text to a given drawable using the current font.
 *
 * This chooses whether to use the legacy or modern font.
 */
void draw_text(xcb_drawable_t drawable, const utf8_t *utf8, uint32_t length,
        uint32_t background_color, const xcb_rectangle_t *rectangle,
        uint32_t foreground_color, int32_t x, int32_t y);

#endif
