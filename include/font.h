#ifndef FONT_H
#define FONT_H

#include <X11/Xft/Xft.h>

#include "utility/types.h"

/* the maximum number of glyphs */
#define MAX_GLYPH_COUNT 1024

/* a glyph sequence */
struct text_item {
    /* the font to use for rendering this item */
    XftFont *font;
    /* the start glyph index within the upper text object */
    int glyph_index;
    /* the number of glyphs */
    int glyph_count;
    /* text extents of this item */
    XGlyphInfo extents;
};

/* a text object holds information of glyph sequences and what font should be
 * used to render them
 */
typedef struct text {
    /* indexes within their respective font */
    FT_UInt *glyphs;
    /* the origin of the first glyph */
    int x;
    int y;
    /* the logical width and height of the text */
    unsigned width;
    unsigned height;
    /* the text sections */
    struct text_item *items;
    /* the number of text sections */
    int item_count;
} Text;

/* Get an Xft color from an RGB color. */
int allocate_xft_color(uint32_t rgb, XftColor *color);

/* Free an Xft color previous allocated by `allocate_xft_color()`. */
void free_xft_color(XftColor *color);

/* Free the resources the font list occupies. */
void free_font_list(void);

/* Set the global font using given fontconfig pattern string. */
int set_font(const char *name);

/* Convert given @utf8 string with @length to a glyph array.
 *
 * @length may be -1, then the function uses strlen() on utf8.
 *
 * @return a pointer to the first glyph.
 *         Do NOT free this pointer.
 *         It is limited to `MAX_GLYPH_COUNT` glyphs.
 */
FcChar32 *get_glyphs(const utf8_t *utf8, int length, int *glyph_count);

/* Transform given glyphs to a text object.
 *
 * Free the resources the text occupies using `clear_text()`.
 */
void initialize_text(Text *text, const FcChar32 *glyphs, int glyph_count);

/* Draw given @text using drawable @draw. */
void draw_text(XftDraw *draw, XftColor *color, int x, int y, Text *text);

/* Clear the resources occupied by a text object. */
void clear_text(Text *text);

#endif
