#include <inttypes.h>

#include "log.h"
#include "render.h"
#include "resources.h"
#include "x11_management.h"
#include "xalloc.h"

/* the font used for rendering */
static struct font {
    /* if font drawing was initialized */
    bool is_initialized;
    /* if the font is available */
    bool is_available;
    /* the freetype library handle */
    FT_Library library;
    /* pen used to render text */
    xcb_render_picture_t pen;
    /* the freetype font faces for rendering */
    FT_Face *faces;
    /* number of freetype font faces */
    uint32_t number_of_faces;
    /* the xcb glyphset containing glyphs */
    xcb_render_glyphset_t glyphset;
    /* we use this to remember which glyphs we added to the glyphset */
    FcCharSet *charset;
} font;

/* a mapping from drawable to picture */
static struct drawable_picture_cache {
    /* the id of the drawable */
    xcb_drawable_t drawable;
    /* the picture created for the drawable */
    xcb_render_picture_t picture;
    /* the next cache object in the linked list */
    struct drawable_picture_cache *next;
} *drawable_picture_cache_head;

/* Check if modern fonts are initialized. */
inline bool has_modern_font_drawing(void)
{
    return font.is_available;
}

/* Create a picture for the given window (or retrieve it from the cache).
 *
 * The cached items do not need to be cleared ever since they are only for the
 * two fensterchef windows (notification and window list) which only get cleared
 * when fensterchef quits.
 */
static xcb_render_picture_t cache_drawable_picture(xcb_drawable_t drawable)
{
    struct drawable_picture_cache *last;
    struct drawable_picture_cache *cache;
    xcb_render_picture_t picture;

    if (drawable_picture_cache_head == NULL) {
        cache = xmalloc(sizeof(*cache));
        drawable_picture_cache_head = cache;
        last = NULL;
    } else {
        for (last = drawable_picture_cache_head; true; last = last->next) {
            if (last->drawable == drawable) {
                return last->picture;
            }
            if (last->next == NULL) {
                break;
            }
        }
        cache = xmalloc(sizeof(*drawable_picture_cache_head));
        last->next = cache;
    }

    cache->drawable = drawable;
    cache->next = NULL;

    /* create a picture for rendering */
    picture = xcb_generate_id(connection);
    general_values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
    general_values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;
    xcb_render_create_picture(connection, picture, drawable,
                find_visual_format(screen->root_visual),
                XCB_RENDER_CP_POLY_MODE | XCB_RENDER_CP_POLY_EDGE,
                general_values);

    cache->picture = picture;
    return picture;
}

/* Set the color of a pen. */
static void set_pen_color(xcb_render_picture_t pen, xcb_render_color_t color)
{
    xcb_rectangle_t rectangle;

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = 1;
    rectangle.height = 1;
    xcb_render_fill_rectangles(connection, XCB_RENDER_PICT_OP_OVER, pen,
        color, 1, &rectangle);
}

/* Create a picture with width and height set to 1. */
static xcb_render_picture_t create_pen(void)
{
    xcb_pixmap_t pixmap;
    xcb_render_picture_t picture;

    /* create 1x1 pixmap */
    pixmap = xcb_generate_id(connection);
    xcb_create_pixmap(connection, screen->root_depth, pixmap, screen->root,
            1, 1);

    /* create repeated picture to render on */
    picture = xcb_generate_id(connection);
    general_values[0] = XCB_RENDER_REPEAT_NORMAL;
    xcb_render_create_picture(connection, picture, pixmap,
            get_picture_format(24), XCB_RENDER_CP_REPEAT, general_values);

    return picture;
}

/* Initializes all parts needed for drawing fonts. */
int initialize_modern_font_drawing(void)
{
    FT_Error ft_error;

    /* initialize the freetype library */
    ft_error = FT_Init_FreeType(&font.library);
    if (ft_error != FT_Err_Ok) {
        LOG_ERROR("could not initialize freetype: %u", ft_error);
        return ERROR;
    }

    /* initialize fontconfig, this reads the font configuration database */
    if (FcInit() == FcFalse) {
        LOG_ERROR("could not initialize fontconfig\n");
        FT_Done_FreeType(font.library);
        return ERROR;
    }

    font.pen = create_pen();

    /* create the glyphset which will store the glyph pixel data */
    font.glyphset = xcb_generate_id(connection);
    xcb_render_create_glyph_set(connection, font.glyphset,
            get_picture_format(8));

    font.is_initialized = true;
    return OK;
}

/* Create a font face using given pattern. */
static FT_Face create_font_face(FcPattern *pattern)
{
    FcValue fc_file, fc_index, fc_matrix, fc_size;
    FT_Face face;
    FT_Error ft_error;
    FT_Matrix matrix;

    /* get the file name of the font */
    if (FcPatternGet(pattern, FC_FILE, 0, &fc_file) != FcResultMatch) {
        LOG_ERROR("could not not get the font file\n");
        FcPatternDestroy(pattern);
        return NULL;
    }

    /* get index of the font within the file (files can have multiple fonts) */
    if (FcPatternGet(pattern, FC_INDEX, 0, &fc_index) != FcResultMatch) {
        fc_index.type = FcTypeInteger;
        fc_index.u.i = 0;
    }

    /* create a new font face */
    ft_error = FT_New_Face(font.library, (const char*) fc_file.u.s,
            fc_index.u.i, &face);
    if (ft_error != FT_Err_Ok) {
        LOG_ERROR("could not not create the new freetype face: %d", ft_error);
        FcPatternDestroy(pattern);
        return NULL;
    }

    /* get the transformation matrix of the font or use a default one */
    if (FcPatternGet(pattern, FC_MATRIX, 0, &fc_matrix) == FcResultMatch) {
        /* fontconfig gives the matrix as values from 0 to 1 but freetype
         * expects 16.16 fixed point format hence why we multiply by 0x10000
         */
        matrix.xx = (FT_Fixed) (fc_matrix.u.m->xx * 0x10000);
        matrix.xy = (FT_Fixed) (fc_matrix.u.m->xy * 0x10000);
        matrix.yx = (FT_Fixed) (fc_matrix.u.m->yx * 0x10000);
        matrix.yy = (FT_Fixed) (fc_matrix.u.m->yy * 0x10000);
        FT_Set_Transform(face, &matrix, NULL);
    }

    /* get the size based one the size or fall back to 12 */
    if (FcPatternGet(pattern, FC_SIZE, 0, &fc_size) != FcResultMatch ||
            fc_size.u.d == 0) {
        fc_size.u.d = 12.0;
    }

    /* select the charmap closest to unicode */
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);

    /* try to set the size of the face, we need to multiply by 64 because
     * FT_Set_Char_Size() expects 26.6 fractional points
     */
    if (FT_Set_Char_Size(face, 0, fc_size.u.d * 64,
                resources.dpi, resources.dpi) != FT_Err_Ok) {
        /* fallback to setting the first availabe size, this must be a font
         * without support for resizing
         */
        FT_Select_Size(face, 0);
    }

    LOG("new font face created from %.*s\n", fc_file.u.i, fc_file.u.s);
    return face;
}

/* Free all data used by the current font. */
static void free_font(void)
{
    /* check if the font is already freed */
    if (!font.is_available) {
        return;
    }

    font.is_available = false;
    for (uint32_t i = 0; i < font.number_of_faces; i++) {
        FT_Done_Face(font.faces[i]);
    }
    free(font.faces);
    font.faces = NULL;
    font.number_of_faces = 0;
    FcCharSetDestroy(font.charset);
}

/* This sets the globally used font for rendering. */
int set_modern_font(const utf8_t *query)
{
    int error = OK;
    FT_Face *faces;
    uint32_t number_of_faces;
    utf8_t *part;
    FcBool status;
    FcPattern *finding_pattern, *pattern;
    FcResult result;
    FT_Face face;

    if (!font.is_initialized) {
        return ERROR;
    }

    /* reload the font configuration if any changed */
    (void) FcInitBringUptoDate();

    faces = NULL;
    number_of_faces = 0;

    while (query[0] != '\0') {
        /* get the next element of the comma separated list */
        part = (utf8_t*) query;
        while (query[0] != '\0' && query[0] != ',') {
            query++;
        }
        part = (utf8_t*) xstrndup((char*) part, query - part);
        if (query[0] == ',') {
            query++;
        }
        while (isspace(query[0])) {
            query++;
        }

        /* parse the query into a matching pattern */
        finding_pattern = FcNameParse(part);
        free(part);

        /* uses the current configuration to fill the finding pattern */
        status = FcConfigSubstitute(NULL, finding_pattern, FcMatchPattern);
        if (status == FcFalse) {
            LOG_ERROR("could not substitute font pattern\n");
            error = ERROR;
            break;
        }
        /* this supplies the pattern with some default values if some are
         * unset
         */
        FcDefaultSubstitute(finding_pattern);

        /* gets the font that matches best with what is requested */
        pattern = FcFontMatch(NULL, finding_pattern, &result);

        FcPatternDestroy(finding_pattern);

        if (result != FcResultMatch) {
            error = ERROR;
            break;
        }

        /* create the font face using the file name contained in the pattern */
        face = create_font_face(pattern);

        /* no longer need the pattern */
        FcPatternDestroy(pattern);

        if (face == NULL) {
            error = ERROR;
            break;
        }

        RESIZE(faces, number_of_faces + 1);
        faces[number_of_faces++] = face;
    }

    if (error != OK || number_of_faces == 0) {
        for (uint32_t i = 0; i < number_of_faces; i++) {
            FT_Done_Face(faces[i]);
        }
        free(faces);
        return ERROR;
    }

    LOG("switching fonts to the %u specified font(s)\n", number_of_faces);

    /* free the old font */
    free_font();

    font.faces = faces;
    font.number_of_faces = number_of_faces;
    font.charset = FcCharSetCreate();
    font.is_available = true;
    return OK;
}

/* Attempts to find a font containing given glyph. */
static FT_Face create_font_face_containing_glyph(uint32_t glyph)
{
	FcBool status;
	FcResult result;
	FcCharSet *charset;
	FcPattern *finding_pattern, *pattern;
	FT_Face face;

	/* create the pattern and font face to hold onto the glyph */
	charset = FcCharSetCreate();
	FcCharSetAddChar(charset, glyph);
	finding_pattern = FcPatternCreate();
	FcPatternAddCharSet(finding_pattern, FC_CHARSET, charset);

    /* uses the current configuration to fill the finding pattern */
	status = FcConfigSubstitute(NULL, finding_pattern, FcMatchPattern);
	if (status == FcFalse) {
		FcCharSetDestroy(charset);
		return NULL;
	}
    /* this supplies the pattern with some default values if some are unset */
	FcDefaultSubstitute(finding_pattern);

    /* gets the font that matches best with what is requested */
	pattern = FcFontMatch(NULL, finding_pattern, &result);

	FcPatternDestroy(finding_pattern);

	if (result != FcResultMatch) {
		FcCharSetDestroy(charset);
		return NULL;
	}

	face = create_font_face(pattern);

	FcCharSetDestroy(charset);

	return face;
}

/* Load a glyph into a face and return the face it was loaded in. */
static FT_Face load_glyph(uint32_t glyph, FT_Int32 load_flags)
{
    FT_UInt glyph_index;
    FT_Face face;

    face = NULL;
    for (uint32_t j = 0; j < font.number_of_faces; j++) {
        glyph_index = FT_Get_Char_Index(font.faces[j], glyph);
        if (glyph_index == 0) {
            continue;
        }
        if (FT_Load_Glyph(font.faces[j], glyph_index, load_flags) != FT_Err_Ok) {
            return NULL;
        }
        face = font.faces[j];
        return face;
    }

    /* glyph was not found, try an alternative font face */
    face = create_font_face_containing_glyph(glyph);
    if (face == NULL) {
        return NULL;
    }

    /* add the face to the font face list */
    RESIZE(font.faces, font.number_of_faces + 1);
    font.faces[font.number_of_faces++] = face;

    glyph_index = FT_Get_Char_Index(face, glyph);
    if (glyph_index == 0) {
        return NULL;
    }
    if (FT_Load_Glyph(face, glyph_index, load_flags) != FT_Err_Ok) {
        return NULL;
    }
    return face;
}

/* Add the glyph to the cache if not already cached. */
static FT_Face cache_glyph(uint32_t glyph)
{
    FT_Face face;
    xcb_render_glyphinfo_t glyph_info;
    uint32_t stride;
    uint8_t *temporary_bitmap;

    if (glyph == 0) {
        return NULL;
    }

    /* check if the glyph is already cached */
    if (FcCharSetHasChar(font.charset, glyph)) {
        face = load_glyph(glyph, FT_LOAD_DEFAULT);
        if (face == NULL) {
            return NULL;
        }
        return face;
    }

    /* find the face that has the glyph and load it */
    face = load_glyph(glyph, FT_LOAD_RENDER);
    if (face == NULL) {
        LOG_VERBOSE("could not load face for glyph: " COLOR(GREEN)
                    "U+%08" PRIx32 "\n",
                glyph);
        return NULL;
    }

    glyph_info.x = -face->glyph->bitmap_left;
    glyph_info.y = face->glyph->bitmap_top;
    glyph_info.width = face->glyph->bitmap.width;
    glyph_info.height = face->glyph->bitmap.rows;
    /* dividing by 64 converts from 26.6 fractional points to pixels */
    glyph_info.x_off = face->glyph->advance.x / 64;
    glyph_info.y_off = face->glyph->advance.y / 64;

    /* round up the width to a multiple of 4, this is the *stride*, the
     * X renderer expects this
     */
    stride = (glyph_info.width + (0x4 - 0x1)) & ~(0x4 - 0x1);
    temporary_bitmap = xcalloc(stride * glyph_info.height,
            sizeof(*temporary_bitmap));

    for (uint16_t y = 0; y < glyph_info.height; y++) {
        memcpy(temporary_bitmap + y * stride,
                face->glyph->bitmap.buffer + y * glyph_info.width,
                glyph_info.width);
    }

    /* add the glyph to the glyph set */
    xcb_render_add_glyphs(connection,
            font.glyphset, 1, &glyph, &glyph_info,
            stride * glyph_info.height, temporary_bitmap);

    free(temporary_bitmap);

    LOG_VERBOSE("cached glyph: " COLOR(GREEN) "U+%08" PRIx32 "\n", glyph);

    /* mark the glyph as cached */
    FcCharSetAddChar(font.charset, glyph);
    return face;
}

/* Draw text using the modern client side rendering. */
void draw_text_modern(xcb_drawable_t drawable, const utf8_t *utf8,
        uint32_t length, uint32_t background_color,
        const xcb_rectangle_t *rectangle, uint32_t foreground_color, 
        int32_t x, int32_t y)
{
    xcb_render_picture_t picture;
    xcb_render_color_t color;
    uint32_t glyph;
    struct {
        struct {
            /* number of glyphs */
            uint8_t count;
            /* NOT USED: internal padding */
            uint8_t struct_padding[3];
            /* position of the glyphs */
            int16_t x, y;
        } header;
        /* the actual glyphs; -1 is needed to prevent an overflow when looping
         * using a `uint8_t`
         */
        uint32_t glyphs[UINT8_MAX - 1];
    } glyphs;
    FT_Face face;
    uint32_t text_width;

    /* get a picture to draw on */
    picture = cache_drawable_picture(drawable);

    convert_color_to_xcb_color(&color, foreground_color);
    set_pen_color(font.pen, color);

    /* fill the background */
    if (rectangle != NULL) {
        convert_color_to_xcb_color(&color, background_color);
        xcb_render_fill_rectangles(connection, XCB_RENDER_PICT_OP_OVER,
                picture, color, 1, rectangle);
    }

    glyphs.header.x = x;
    glyphs.header.y = y;
    /* load the glyphs and send them in chunks to the X server */
    for (uint32_t i = 0; i < length; ) {
        text_width = 0;
        /* iterate over all glyphs and load them */
        glyphs.header.count = 0;
        while (glyphs.header.count < SIZE(glyphs.glyphs) && i < length) {
            U8_NEXT(utf8, i, length, glyph);

            face = cache_glyph(glyph);
            if (face == NULL) {
                continue;
            }

            glyphs.glyphs[glyphs.header.count++] = glyph;

            /* dividing by 64 converts from 26.6 fractional points to pixels */
            text_width += face->glyph->advance.x / 64;
        }

        /* send a render request to the X renderer */
        xcb_render_composite_glyphs_32(connection,
                XCB_RENDER_PICT_OP_OVER, /* C = Ca + Cb * (1 - Aa) */
                font.pen, /* source picture */
                picture, /* destination picture */
                0, /* mask format */
                font.glyphset,
                0, 0, /* source position */
                sizeof(glyphs.header) +
                    sizeof(glyphs.glyphs[0]) * glyphs.header.count,
                (uint8_t*) &glyphs);

        glyphs.header.x += text_width;
    }
}

/* Measure a text that has no new lines. */
void measure_text_modern(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure)
{
    uint32_t glyph;
    FT_Face face;
    FT_Short ascent, descent;

    measure->ascent = 0;
    measure->descent = 0;
    measure->total_width = 0;

    /* iterate over all glyphs */
    for (uint32_t i = 0; i < length; ) {
        U8_NEXT(utf8, i, length, glyph);

        /* load the char into the font */
        face = cache_glyph(glyph);
        if (face == NULL) {
            continue;
        }

        /* dividing by 64 converts from 26.6 fractional points to pixels */
        measure->total_width += face->glyph->advance.x / 64;
        ascent = face->size->metrics.ascender / 64;
        descent = face->size->metrics.descender / 64;

        measure->ascent = MAX(measure->ascent, ascent);
        measure->descent = MIN(measure->descent, descent);
    }
    measure->total_height = measure->ascent - measure->descent;
}
