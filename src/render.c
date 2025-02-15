#include <inttypes.h>

#include <xcb/xcb_renderutil.h>

#include "fensterchef.h"
#include "log.h"
#include "render.h"
#include "screen.h"
#include "utf8.h"
#include "utility.h"

/* TODO: how do we make colored emojis work?
 * I tried to but color formats other than 8 bit colors
 */

/* the render formats for pictures */
static xcb_render_query_pict_formats_reply_t *formats;

/* graphical objects with the id referring to the xcb id */
uint32_t stock_objects[STOCK_MAX];

/* the font used for rendering */
static struct font {
    /* if font drawing is available */
    FcBool available;
    /* the freetype library handle */
    FT_Library library;
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
static struct window_picture_cache {
    xcb_drawable_t xcb_drawable;
    xcb_render_picture_t picture;
    struct window_picture_cache *next;
} *window_picture_cache_head;

/* Get the format of a visual. */
static xcb_render_pictformat_t find_visual_format(xcb_visualid_t visual)
{
    xcb_render_pictscreen_iterator_t screens;
    xcb_render_pictdepth_iterator_t depths;
    xcb_render_pictvisual_iterator_t visuals;

    /* for each screen */
    for (screens = xcb_render_query_pict_formats_screens_iterator(formats);
            screens.rem > 0; xcb_render_pictscreen_next(&screens)) {
        /* for each screen's supported depths */
	    for (depths = xcb_render_pictscreen_depths_iterator(screens.data);
	            depths.rem > 0; xcb_render_pictdepth_next(&depths)) {
            /* for each supported depth's visuals  */
	        for (visuals = xcb_render_pictdepth_visuals_iterator(depths.data);
	                visuals.rem > 0; xcb_render_pictvisual_next(&visuals)) {
		        if (visuals.data->visual == visual) {
		            return visuals.data->format;
		        }
		    }
		}
	}
    return 0;
}

/* Get a good picture format for given depth. */
static xcb_render_pictformat_t get_picture_format(uint8_t depth)
{
    xcb_render_pictforminfo_iterator_t i;

    /* iterate over all picture formats and pick the one with needed depth */
    for (i = xcb_render_query_pict_formats_formats_iterator(formats);
            i.rem > 0; xcb_render_pictforminfo_next(&i)) {
        /* the direct type is for storing colors per pixel without a palette */
        if (i.data->depth == depth &&
                i.data->type == XCB_RENDER_PICT_TYPE_DIRECT) {
            return i.data->id;
        }
    }
    return 0;
}

/* Free all data used by the font. */
static void free_font(void)
{
    for (uint32_t i = 0; i < font.number_of_faces; i++) {
        FT_Done_Face(font.faces[i]);
    }
    free(font.faces);
    font.faces = NULL;
    font.number_of_faces = 0;
    xcb_render_free_glyph_set(connection, font.glyphset);
    FcCharSetDestroy(font.charset);
}

/* Initialize the graphical stock objects that can be used for rendering. */
int initialize_renderer(void)
{
    xcb_render_query_pict_formats_cookie_t formats_cookie;
    xcb_drawable_t root;
    xcb_generic_error_t *error;
    xcb_render_color_t color;
    xcb_render_picture_t pen;

    /* the picture formats are the possible ways colors can be represented,
     * for example ARGB, 8 bit colors etc.
     */
    formats_cookie = xcb_render_query_pict_formats(connection);
    formats = xcb_render_query_pict_formats_reply(connection,
            formats_cookie, 0);
    if (formats == NULL) {
        LOG_ERROR(NULL, "could not query picture formats");
        return ERROR;
    }

    root = screen->xcb_screen->root;

    for (uint32_t i = 0; i < STOCK_MAX; i++) {
        stock_objects[i] = xcb_generate_id(connection);
    }

    /* create a graphics context */
    general_values[0] = screen->xcb_screen->black_pixel;
    general_values[1] = screen->xcb_screen->white_pixel;
    error = xcb_request_check(connection,
            xcb_create_gc_checked(connection, stock_objects[STOCK_GC],
                root, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, general_values));
    if (error != NULL) {
        LOG_ERROR(error, "could not create graphics context for notifications");
        return ERROR;
    }

    /* create a graphics context with inverted colors */
    general_values[0] = screen->xcb_screen->white_pixel;
    general_values[1] = screen->xcb_screen->black_pixel;
    error = xcb_request_check(connection,
            xcb_create_gc_checked(connection,
                stock_objects[STOCK_INVERTED_GC], root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, general_values));
    if (error != NULL) {
        LOG_ERROR(error, "could not create inverted graphics context for notifications");
        return ERROR;
    }

    /* create a white pen */
    convert_color_to_xcb_color(&color, screen->xcb_screen->white_pixel);
    pen = create_pen(color);
    if (pen == XCB_NONE) {
        return ERROR;
    }
    stock_objects[STOCK_WHITE_PEN] = create_pen(color);

    /* create a black pen */
    convert_color_to_xcb_color(&color, screen->xcb_screen->black_pixel);
    pen = create_pen(color);
    if (pen == XCB_NONE) {
        return ERROR;
    }
    stock_objects[STOCK_BLACK_PEN] = pen;

    return OK;
}

/* Frees all resources associated to rendering. */
void deinitialize_renderer(void)
{
    struct window_picture_cache *cache, *next;

    for (cache = window_picture_cache_head;
            cache != NULL; cache = next) {
        next = cache->next;
        xcb_render_free_picture(connection, cache->picture);
        free(cache);
    }

    xcb_render_free_picture(connection, stock_objects[STOCK_WHITE_PEN]);
    xcb_render_free_picture(connection, stock_objects[STOCK_BLACK_PEN]);
    xcb_free_gc(connection, stock_objects[STOCK_GC]);
    xcb_free_gc(connection, stock_objects[STOCK_INVERTED_GC]);

    if (!font.available) {
        return;
    }

    free_font();

    FT_Done_FreeType(font.library);
    FcFini();
}

/* Creates a picture for the given window (or retrieves it from the cache). */
xcb_render_picture_t cache_window_picture(xcb_drawable_t xcb_drawable)
{
    struct window_picture_cache *last;
    struct window_picture_cache *cache;
    xcb_generic_error_t *error;
    xcb_render_picture_t picture;

    if (window_picture_cache_head == NULL) {
        cache = xmalloc(sizeof(*cache));
        window_picture_cache_head = cache;
        last = NULL;
    } else {
        for (last = window_picture_cache_head; true; last = last->next) {
            if (last->xcb_drawable == xcb_drawable) {
                return last->picture;
            }
            if (last->next == NULL) {
                break;
            }
        }
        cache = xmalloc(sizeof(*window_picture_cache_head));
        last->next = cache;
    }

    cache->xcb_drawable = xcb_drawable;
    cache->next = NULL;

    /* create a picture for rendering */
    picture = xcb_generate_id(connection);
    general_values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
    general_values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;
    error = xcb_request_check(connection,
                xcb_render_create_picture_checked(connection, picture,
                xcb_drawable,
                find_visual_format(screen->xcb_screen->root_visual),
                XCB_RENDER_CP_POLY_MODE | XCB_RENDER_CP_POLY_EDGE,
                general_values));
    if (error != NULL) {
        LOG_ERROR(error, "could not create picture");
        free(cache);
        if (last != NULL) {
            last->next = NULL;
        }
        return XCB_NONE;
    }

    cache->picture = picture;
    return picture;
}

/* Set the color of a pen. */
void set_pen_color(xcb_render_picture_t pen, xcb_render_color_t color)
{
    xcb_rectangle_t rectangle;

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = 1;
    rectangle.height = 1;
    xcb_render_fill_rectangles(connection, XCB_RENDER_PICT_OP_OVER, pen,
        color, 1, &rectangle);
}

/* Creates a picture with width and height set to 1. */
xcb_render_picture_t create_pen(xcb_render_color_t color)
{
    xcb_pixmap_t pixmap;
    xcb_generic_error_t *error;
    xcb_render_picture_t picture;

    /* create 1x1 pixmap */
    pixmap = xcb_generate_id(connection);
    error = xcb_request_check(connection, xcb_create_pixmap_checked(connection,
                screen->xcb_screen->root_depth, pixmap,
                screen->xcb_screen->root, 1, 1));
    if (error != NULL) {
        LOG_ERROR(error, "could not create pixmap");
        return XCB_NONE;
    }

    /* create repeated picture to render on */
    picture = xcb_generate_id(connection);
    general_values[0] = XCB_RENDER_REPEAT_NORMAL;
    error = xcb_request_check(connection,
            xcb_render_create_picture_checked(connection, picture, pixmap,
                get_picture_format(24), XCB_RENDER_CP_REPEAT, general_values));
    if (error != NULL) {
        LOG_ERROR(error, "could not create picture");
        return XCB_NONE;
    }

    set_pen_color(picture, color);
    return picture;
}

/* Initializes all parts needed for drawing fonts. */
int initialize_font_drawing(void)
{
    FT_Error error;

    /* initialize the freetype library */
    error = FT_Init_FreeType(&font.library);
    if (error != FT_Err_Ok) {
        LOG_ERROR(NULL, "could not initialize freetype: %u", error);
        return ERROR;
    }

    /* initialize fontconfig, this reads the font configuration database */
    if (FcInit() == FcFalse) {
        LOG_ERROR(NULL, "could not initialize freetype: %u", error);
        FT_Done_FreeType(font.library);
        return ERROR;
    }

    font.available = true;
    return OK;
}

/* Create a font face using given pattern. */
static FT_Face create_font_face(FcPattern *pattern)
{
    FcValue fc_file, fc_index, fc_matrix, fc_size;
    FT_Face face;
    FT_Error ft_error;
    FT_Matrix matrix;
    FT_UInt horizontal_dpi, vertical_dpi;

    /* get the file name of the font */
    if (FcPatternGet(pattern, FC_FILE, 0, &fc_file) != FcResultMatch) {
        LOG_ERROR(NULL, "could not not get the font file");
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
        LOG_ERROR(NULL, "could not not create the new freetype face: %d",
                ft_error);
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

    /* compute the dpi (dots per inch) with the pixel size and millimeter size,
     * multiplying by 254 and then diving by 10 converts the millimeters to
     * inches, adding half the divisor before dividing will round better
     */
    horizontal_dpi = (FT_UInt) (screen->xcb_screen->width_in_pixels * 254 +
            screen->xcb_screen->width_in_millimeters * 5) /
        (screen->xcb_screen->width_in_millimeters * 10);
    vertical_dpi = (FT_UInt) (screen->xcb_screen->height_in_pixels * 254 +
            screen->xcb_screen->height_in_millimeters * 5) /
        (screen->xcb_screen->height_in_millimeters * 10);
    /* try to set the size of the face, we need to multiply by 64 because
     * FT_Set_Char_Size() expects 26.6 fractional points
     */
    if (FT_Set_Char_Size(face, 0, fc_size.u.d * 64,
                horizontal_dpi, vertical_dpi) != FT_Err_Ok) {
        /* fallback to setting the first availabe size, this must be a font
         * without support for resizing
         */
        FT_Select_Size(face, 0);
    }

    LOG("new font face created from '%.*s'\n", fc_file.u.i, fc_file.u.s);
    return face;
}

/* This sets the globally used font for rendering. */
int set_font(const utf8_t *query)
{
    utf8_t *part;
    FcBool status;
    FcPattern *finding_pattern, *pattern;
    FcResult result;
    FT_Face face;
    FT_Face *faces;
    uint32_t number_of_faces;
    xcb_render_glyphset_t glyphset;
    xcb_generic_error_t *error;

    if (!font.available) {
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
            LOG_ERROR(NULL, "could not substitute font pattern");
            return ERROR;
        }
        /* this supplies the pattern with some default values if some are
         * unset
         */
        FcDefaultSubstitute(finding_pattern);

        /* gets the font that matches best with what is requested */
        pattern = FcFontMatch(NULL, finding_pattern, &result);

        FcPatternDestroy(finding_pattern);

        if (result != FcResultMatch) {
            LOG_ERROR(NULL, "there was no matching font");
            return ERROR;
        }

        /* create the font face using the file name contained in the pattern */
        face = create_font_face(pattern);

        /* no longer need the pattern */
        FcPatternDestroy(pattern);

        if (face == NULL) {
            return ERROR;
        }

        /* create the glyphset which will store the glyph pixel data */
        glyphset = xcb_generate_id(connection);
        error = xcb_request_check(connection,
                    xcb_render_create_glyph_set_checked(connection,
                        glyphset, get_picture_format(8)));
        if (error != NULL) {
            LOG_ERROR(error, "could not create the glyphset");
            FT_Done_Face(face);
            return ERROR;
        }

        RESIZE(faces, number_of_faces + 1);
        faces[number_of_faces++] = face;
    }

    LOG("switching fonts to the %" PRIu32 " specified font(s)\n",
            number_of_faces);

    /* free the old font */
    free_font();

    font.faces = faces;
    font.number_of_faces = number_of_faces;
    font.glyphset = glyphset;
    font.charset = FcCharSetCreate();
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

/* Add the glyph to the cache. The caller must have made sure that the glyph is
 * not already cached.
 */
static int cache_glyph(uint32_t glyph, FT_Pos *advance)
{
    FT_Face face;
    xcb_render_glyphinfo_t glyph_info;
    uint32_t stride;
    uint8_t *temporary_bitmap;
    xcb_generic_error_t *error;

    if (glyph == 0) {
        return ERROR;
    }

    /* find the face that has the glyph and load it */
    face = load_glyph(glyph, FT_LOAD_RENDER);
    if (face == NULL) {
        return ERROR;
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
    error = xcb_request_check(connection,
            xcb_render_add_glyphs_checked(connection,
                font.glyphset, 1, &glyph, &glyph_info,
                stride * glyph_info.height, temporary_bitmap));
    free(temporary_bitmap);

    if (error != NULL) {
        LOG_ERROR(error, "could not add glyph U+%8x to the glyphset",
                (unsigned) glyph);
        return ERROR;
    }

    *advance = glyph_info.x_off;

    /* mark the glyph as cached */
    FcCharSetAddChar(font.charset, glyph);
    return OK;
}

/* This draws text to a given picture using the current font. */
int draw_text(xcb_drawable_t xcb_drawable, const utf8_t *utf8, uint32_t length,
        xcb_render_color_t background_color, const xcb_rectangle_t *rectangle,
        xcb_render_picture_t foreground, int32_t x, int32_t y)
{
    xcb_render_picture_t picture;
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
        /* the actual glyphs */
        uint32_t glyphs[UINT8_MAX - 1];
    } glyphs;
    FT_Pos advance;
    uint32_t text_width;
    xcb_generic_error_t *error = NULL;

    if (!font.available) {
        return ERROR;
    }

    /* get a picture to draw on */
    picture = cache_window_picture(xcb_drawable);
    if (picture == XCB_NONE) {
        return ERROR;
    }

    if (rectangle != NULL) {
        xcb_render_fill_rectangles(connection, XCB_RENDER_PICT_OP_OVER,
                picture, background_color, 1, rectangle);
    }

    glyphs.header.x = x;
    glyphs.header.y = y;
    for (uint32_t i = 0; i < length; ) {
        text_width = 0;
        /* iterate over all glyphs and load them */
        glyphs.header.count = 0;
        while (glyphs.header.count < SIZE(glyphs.glyphs) && i < length) {
            U8_NEXT(utf8, i, length, glyph);

            if (FcCharSetHasChar(font.charset, glyph)) {
                glyphs.glyphs[glyphs.header.count++] = glyph;
                continue;
            }

            if (cache_glyph(glyph, &advance) == ERROR) {
                continue;
            }

            glyphs.glyphs[glyphs.header.count++] = glyph;

            text_width += advance;
        }

        /* send a render request to the x renderer */
        error = xcb_request_check(connection,
                    xcb_render_composite_glyphs_32_checked(connection,
                        XCB_RENDER_PICT_OP_OVER, /* C = Ca + Cb * (1 - Aa) */
                        foreground, /* source picture */
                        picture, /* destination picture */
                        0, /* mask format */
                        font.glyphset,
                        0, 0, /* source position */
                        sizeof(glyphs.header) +
                            sizeof(uint32_t) * glyphs.header.count,
                        (uint8_t*) &glyphs));
        if (error != NULL) {
            LOG_ERROR(error, "could not render composite glyphs");
            break;
        }

        glyphs.header.x += text_width;
    }

    return error == NULL ? OK : ERROR;
}

/* Measure a text that has no new lines. */
void measure_text(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure)
{
    uint32_t glyph;
    FT_Face face;
    FT_Short ascent, descent;

    measure->ascent = 0;
    measure->descent = 0;
    measure->total_width = 0;

    if (!font.available) {
        return;
    }

    /* iterate over all glyphs */
    for (uint32_t i = 0; i < length; ) {
        U8_NEXT(utf8, i, length, glyph);
        /* load the char into the font */
        face = load_glyph(glyph, FT_LOAD_DEFAULT);
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
}
