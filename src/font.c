#include <inttypes.h>

#include "configuration.h"
#include "font.h"
#include "log.h"
#include "utility/utility.h"
#include "x11_synchronize.h"

/* a list of known fonts */
struct font {
    /* the pattern of the font */
    FcPattern *pattern;
    /* the loaded font object, may be NULL to indicate it was not loaded yet */
    XftFont *font;
};

/* list of the currently used fonts */
static struct font_list {
    /* all fonts within the font list */
    struct font *fonts;
    /* the number of fonts in the font list */
    int count;
} font_list;

/* Get an xft color from an RGB color. */
int allocate_xft_color(uint32_t rgb, XftColor *color)
{
    XRenderColor render_color;
    Bool result;

    render_color.red = ((rgb >> 8) & 0xff00);
    render_color.green = (rgb & 0xff00);
    render_color.blue = (rgb & 0xff) << 8;
    render_color.alpha = 0xffff;
    result = XftColorAllocValue(display,
            DefaultVisual(display, DefaultScreen(display)),
            DefaultColormap(display, DefaultScreen(display)),
            &render_color, color);
    if (!result) {
        LOG_ERROR("could not allocate Xft color value for background\n");
        return ERROR;
    }

    return OK;
}

/* Free an Xft color previous allocated by `allocate_xft_color()`. */
void free_xft_color(XftColor *color)
{
    XftColorFree(display,
            DefaultVisual(display, DefaultScreen(display)),
            DefaultColormap(display, DefaultScreen(display)),
            color);
}

/* Free the resources the font list occupies. */
void free_font_list(void)
{
    for (int i = 0; i < font_list.count; i++) {
        if (font_list.fonts[i].font != NULL) {
            XftFontClose(display, font_list.fonts[i].font);
        }
        FcPatternDestroy(font_list.fonts[i].pattern);
    }
    free(font_list.fonts);
}

/* Set the global font using given fontconfig pattern string. */
int set_font(const utf8_t *name)
{
    FcPattern *pattern;
    FcPattern *match;
    FcResult result;
    FcFontSet *set;

    struct font *fonts;
    int font_count = 0;

    /* reload font configuration if anything changed */
    (void) FcInitBringUptoDate();

    pattern = XftNameParse(name);
    if (pattern == NULL) {
        LOG_ERROR("could not parse font name: %s\n",
                name);
        return ERROR;
    }

    match = XftFontMatch(display, DefaultScreen(display), pattern, &result);
    FcPatternDestroy(pattern);

    if (match == NULL) {
        LOG_ERROR("could not match font name: %s\n",
                name);
        return ERROR;
    }

    /* get a set of fonts covering most unicode glyphs */
    set = FcFontSort(NULL, match, FcTrue, NULL, &result);
    if (set == NULL) {
        FcPatternDestroy(match);
        LOG_ERROR("could not create FcFontSet for font creation\n");
        return ERROR;
    }

    /* allocate the fonts to hold onto the patterns */
    ALLOCATE_ZERO(fonts, set->nfont);

    for (int i = 0; i < set->nfont; i++) {
        fonts[i].pattern = FcFontRenderPrepare(NULL, match, set->fonts[i]);
        if (fonts[i].pattern == NULL) {
            font_count = -1;
            /* dereference all previous font patterns */
            for (int j = 0; j < i; j++) {
                FcPatternDestroy(fonts[j].pattern);
            }
            break;
        }
        font_count++;
    }

    FcPatternDestroy(match);
    FcFontSetDestroy(set);

    if (font_count < 0) {
        LOG_ERROR("could not create font patterns for font rendering\n");
        free(fonts);
        return ERROR;
    }

    /* free the old font list */
    free_font_list();

    font_list.fonts = fonts;
    font_list.count = font_count;

    LOG("got %d fonts from %s\n",
            font_count, name);
    return OK;
}

/* Convert given @utf8 string with @length to a glyph array.  */
FcChar32 *get_glyphs(const utf8_t *utf8, int length, int *glyph_count)
{
    static FcChar32 glyphs[MAX_GLYPH_COUNT];

    int index = 0;

    if (length < 0) {
        length = strlen(utf8);
    }

    /* go over each unicode codepoint of the UTF-8 string */
    while (length > 0) {
        const int n = FcUtf8ToUcs4((FcChar8*) utf8, &glyphs[index], length);
        if (n <= 0) {
            break;
        }
        index++;
        if (index == sizeof(glyphs)) {
            LOG_VERBOSE("ran out of glyph space, have %d more bytes\n",
                    length);
            break;
        }
        utf8 += n;
        length -= n;
    }
    *glyph_count = index;
    return glyphs;
}

/* Transform given glyphs to a text object. */
void initialize_text(Text *text, const FcChar32 *glyphs, int glyph_count)
{
    int item_capacity = 2;
    int glyph_index = 0;

    /* initialize the text object */
    ALLOCATE(text->glyphs, glyph_count);
    ALLOCATE(text->items, item_capacity);
    text->item_count = 0;
    text->x = 0;
    text->y = 0;
    text->width = 0;
    text->height = 0;

    /* go over all glyphs */
    for (int i = 0, j; i < glyph_count; i++) {
        const FcChar32 glyph = glyphs[i];
        /* find the font that has this glyph */
        for (j = 0; j < font_list.count; j++) {
            FcCharSet *charset;

            FcPattern *const pattern = font_list.fonts[j].pattern;
            if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &charset) !=
                    FcResultMatch) {
                LOG_DEBUG("font at %d has no char set\n",
                        j);
                continue;
            }

            if (FcCharSetHasChar(charset, glyph)) {
                break;
            }
        }

        if (j == font_list.count) {
            LOG_DEBUG("no font has glyph %#08" PRIx32 "\n",
                    glyph);
            continue;
        }

        /* create the font if not already created */
        if (font_list.fonts[j].font == NULL) {
            FcResult result;
            FcPattern *match;

            match = XftFontMatch(display, DefaultScreen(display),
                    font_list.fonts[j].pattern, &result);
            if (match != NULL) {
                font_list.fonts[j].font = XftFontOpenPattern(display, match);
                FcPatternDestroy(match);
            }
        }

        /* get the font and do a sanity check */
        XftFont *const font = font_list.fonts[j].font;
        if (font == NULL) {
            LOG_ERROR("could not open font for glyph: %#08" PRIx32 "\n",
                    glyph);
            continue;
        }

        /* if no items are there yet, add a first one; add a next one if the
         * fonts mismatch
         */
        if (text->item_count == 0 ||
                text->items[text->item_count - 1].font != font) {
            if (text->item_count == item_capacity) {
                item_capacity *= 2;
                REALLOCATE(text->items, item_capacity);
            }
            /* initialize the item */
            text->items[text->item_count].font = font;
            text->items[text->item_count].glyph_index = glyph_index;
            text->items[text->item_count].glyph_count = 1;
            text->item_count++;
        } else {
            /* simply increment the glyph count to increase the item size */
            text->items[text->item_count - 1].glyph_count++;
        }

        text->glyphs[glyph_index] = XftCharIndex(display, font, glyph);
        glyph_index++;
    }

    if (text->item_count > 1) {
        LOG_DEBUG("text was split into %d items\n",
                text->item_count);
        for (int i = 1; i < text->item_count; i++) {
            LOG_DEBUG("split from glyph %#l" PRIx32 "\n",
                    glyphs[text->items[i].glyph_index]);
        }
    }

    /* compute all glyph extents and the text bounds and offset */
    for (int i = 0; i < text->item_count; i++) {
        struct text_item *const item = &text->items[i];
        XftGlyphExtents(display, item->font, &text->glyphs[item->glyph_index],
                item->glyph_count, &item->extents);

        if (i == 0) {
            text->x = item->extents.x;
            text->y = text->items[0].font->ascent;
        }
        if (i + 1 == text->item_count) {
            text->width += item->extents.width;
        } else {
            text->width += item->extents.xOff;
        }
        text->height = MAX(text->height, (unsigned) item->font->height);

        LOG_DEBUG("extent for item %d: %S %P %P\n",
                i, item->extents.width, item->extents.height,
                item->extents.x, item->extents.y,
                item->extents.xOff, item->extents.yOff);
    }

    LOG_DEBUG("text bounds: %R\n",
            text->x, text->y, text->width, text->height);
}

/* Draw given @text using drawable @draw. */
void draw_text(XftDraw *draw, XftColor *color, int start_x, int start_y,
        Text *text)
{
    int x, y;

    x = start_x;
    y = start_y;
    /* draw each glyph section with the respective item font */
    for (int i = 0; i < text->item_count; i++) {
        struct text_item *const item = &text->items[i];

        /* TODO: align baselines? */

        XftDrawGlyphs(draw, color, item->font, x, y,
                &text->glyphs[item->glyph_index], item->glyph_count);

        x += item->extents.xOff;
    }
}

/* Clear the resources occupied by a text object. */
void clear_text(Text *text)
{
    free(text->glyphs);
    free(text->items);
}
