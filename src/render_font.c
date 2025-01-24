#include <xcb/xcb_renderutil.h>

#include "fensterchef.h"
#include "log.h"
#include "utf8.h"
#include "xalloc.h"

struct font {
    FT_Library library;
    FT_Face face;
    xcb_render_glyphset_t glyphset;
} font;

/* Free all data used by the font besides the FT library handle. */
static void free_font(void)
{
    FT_Done_Face(font.face);
    xcb_render_free_glyph_set(g_dpy, font.glyphset);
}

/* Initializes all parts needed for drawing fonts. */
int init_font_drawing(void)
{
    FT_Error error;

    error = FT_Init_FreeType(&font.library);
    if (error != 0) {
        ERR("could not initialize freetype: %u\n", error);
        return 1;
    }

    if (FcInit() == FcFalse) {
        ERR("could not initialize freetype: %u\n", error);
        FT_Done_FreeType(font.library);
        return 1;
    }

    return 0;
}

/* Frees all resources associated to font rendering. */
void deinit_font_drawing(void)
{
    free_font();
    FT_Done_FreeType(font.library);
    FcFini();
}

/* This sets the globally used font for rendering. */
int set_font(const FcChar8 *query)
{
    FcBool      status;
    FcPattern   *fc_finding_pattern, *pattern;
    FcResult    result;
    FcValue     fc_file, fc_index, fc_matrix, fc_pixel_size;
    FT_Face     face;
    FT_Error    ft_error;
    FT_Matrix   matrix;
    xcb_render_glyphset_t                       glyphset;
    const xcb_render_query_pict_formats_reply_t *fmt_reply;
    xcb_render_pictforminfo_t                   *a8_format;

    if (query == NULL) {
        query = UTF8_TEXT("Noto Sans:size=16");
    }

    fc_finding_pattern = FcNameParse(query);

    status = FcConfigSubstitute(NULL, fc_finding_pattern, FcMatchPattern);
    if (status == FcFalse) {
        ERR("could not substitute font pattern\n");
        return 1;
    }
    FcDefaultSubstitute(fc_finding_pattern);

    pattern = FcFontMatch(NULL, fc_finding_pattern, &result);

    FcPatternDestroy(fc_finding_pattern);

    if (result == FcResultNoMatch) {
        ERR("could not match the font\n");
        return 1;
    }

    if (FcPatternGet(pattern, FC_FILE, 0, &fc_file) != FcResultMatch) {
        ERR("could not not get the font file\n");
        FcPatternDestroy(pattern);
        return 1;
    }

    if (FcPatternGet(pattern, FC_INDEX, 0, &fc_index) != FcResultMatch) {
        fc_index.type = FcTypeInteger;
        fc_index.u.i = 0;
    }

    ft_error = FT_New_Face(font.library, (const char*) fc_file.u.s, fc_index.u.i,
            &face);
    if (ft_error != FT_Err_Ok) {
        ERR("could not not create the new freetype face: %d\n", ft_error);
        FcPatternDestroy(pattern);
        return 1;
    }

    if (FcPatternGet(pattern, FC_MATRIX, 0, &fc_matrix) == FcResultMatch) {
        matrix.xx = (FT_Fixed) (fc_matrix.u.m->xx * 0x10000L);
        matrix.xy = (FT_Fixed) (fc_matrix.u.m->xy * 0x10000L);
        matrix.yx = (FT_Fixed) (fc_matrix.u.m->yx * 0x10000L);
        matrix.yy = (FT_Fixed) (fc_matrix.u.m->yy * 0x10000L);
        FT_Set_Transform(face, &matrix, NULL);
    }

    if (FcPatternGet(pattern, FC_PIXEL_SIZE, 0,
                &fc_pixel_size) != FcResultMatch || fc_pixel_size.u.d == 0) {
        fc_pixel_size.type = FcTypeInteger;
        fc_pixel_size.u.d = 12;
    }

    if (FT_Set_Char_Size(face, 0, fc_pixel_size.u.d * 72 / 96 * 64,
                96, 96) != FT_Err_Ok) {
        ERR("could not set the character size\n");
    }

    fmt_reply = xcb_render_util_query_formats(g_dpy);
    if (fmt_reply == NULL) {
        ERR("could not query for formats\n");
        goto err;
    }

    a8_format = xcb_render_util_find_standard_format(fmt_reply,
            XCB_PICT_STANDARD_A_8);

    if (a8_format == NULL) {
        ERR("could not get the a8 standard format\n");
        goto err;
    }

    glyphset = xcb_generate_id(g_dpy);
    if (xcb_request_check(g_dpy,
                xcb_render_create_glyph_set_checked(g_dpy,
                    glyphset,
                    a8_format->id)) != NULL) {
        ERR("could not create the glyphset\n");
        goto err;
    }

    LOG("switched to font '%s'\n", (char*) query);

    font.face = face;
    font.glyphset = glyphset;
    return 0;

err:
    FcPatternDestroy(pattern);
    FT_Done_Face(face);
    return 1;
}

xcb_render_picture_t create_pen(xcb_render_color_t color)
{
    xcb_render_pictforminfo_t                   *fmt;
    const xcb_render_query_pict_formats_reply_t *fmt_reply;
    xcb_drawable_t                              root;
    xcb_pixmap_t                                pixmap;
    xcb_render_picture_t                        picture;
    xcb_rectangle_t                             rect;

    fmt_reply = xcb_render_util_query_formats(g_dpy);

    fmt = xcb_render_util_find_standard_format(fmt_reply,
            XCB_PICT_STANDARD_ARGB_32);

    root = SCREEN(g_screen_no)->root;

    pixmap = xcb_generate_id(g_dpy);
    xcb_create_pixmap(g_dpy, 32, pixmap, root, 1, 1);

    g_values[0] = XCB_RENDER_REPEAT_NORMAL;
    picture = xcb_generate_id(g_dpy);
    xcb_render_create_picture(g_dpy, picture, pixmap, fmt->id,
            XCB_RENDER_CP_REPEAT, g_values);

    rect.x = 0;
    rect.y = 0;
    rect.width = 1;
    rect.height = 1;
    xcb_render_fill_rectangles(g_dpy, XCB_RENDER_PICT_OP_OVER, picture,
        color, 1, &rect);

    xcb_free_pixmap(g_dpy, pixmap);
    return picture;
}

/* This draws text to a given drawable using the current font. */
void draw_text(xcb_drawable_t xcb_drawable, const FcChar8 *utf8, uint32_t len,
        xcb_render_color_t background_color, xcb_rectangle_t *rect,
        xcb_render_picture_t foreground, int32_t x, int32_t y)
{
    uint32_t                                    uc;
    uint32_t                                    num_glyphs = 0;
    uint32_t                                    *glyphs;
    FT_Bitmap                                   *bitmap;
    xcb_render_glyphinfo_t                      ginfo;
    int                                         stride;
    uint8_t                                     *tmp_bitmap;
    xcb_generic_error_t                         *error;
    xcb_render_util_composite_text_stream_t     *text_stream;
    xcb_render_picture_t                        picture;
    xcb_render_pictforminfo_t                   *fmt;
    const xcb_render_query_pict_formats_reply_t *fmt_reply;

    /* load all glyphs */
    for (uint32_t i = 0; i < len; ) {
        U8_NEXT(utf8, i, len, uc);
        num_glyphs++;
        /* TODO: give font multiple faces */
        if (FT_Load_Char(font.face, uc, FT_LOAD_RENDER |
                FT_LOAD_FORCE_AUTOHINT) != 0) {
            continue;
        }

        bitmap = &font.face->glyph->bitmap;

        ginfo.x = -font.face->glyph->bitmap_left;
        ginfo.y = font.face->glyph->bitmap_top;
        ginfo.width = bitmap->width;
        ginfo.height = bitmap->rows;
        ginfo.x_off = font.face->glyph->advance.x / 64;
        ginfo.y_off = font.face->glyph->advance.y / 64;

        stride = (ginfo.width + 3) & ~3;
        tmp_bitmap = calloc(stride * ginfo.height, sizeof(*tmp_bitmap));

        for (int y = 0; y < ginfo.height; y++) {
            memcpy(tmp_bitmap + y * stride,
                    bitmap->buffer + y * ginfo.width, ginfo.width);
        }

        error = xcb_request_check(g_dpy, xcb_render_add_glyphs_checked(g_dpy,
            font.glyphset, 1, &uc, &ginfo, stride * ginfo.height, tmp_bitmap));
        if (error != NULL) {
            ERR("error adding glyph: %d\n", error->error_code);
        }

        free(tmp_bitmap);
    }
    xcb_flush(g_dpy);

    fmt_reply = xcb_render_util_query_formats(g_dpy);

    fmt = xcb_render_util_find_standard_format(fmt_reply,
            XCB_PICT_STANDARD_RGB_24);

    picture = xcb_generate_id(g_dpy);
    g_values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
    g_values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;
    error = xcb_request_check(g_dpy,
                xcb_render_create_picture_checked(g_dpy, picture,
                xcb_drawable, fmt->id,
                XCB_RENDER_CP_POLY_MODE | XCB_RENDER_CP_POLY_EDGE, g_values));
     if (error != NULL) {
        ERR("could not create picture: %d\n", error->error_code);
        return;
    }

     glyphs = xreallocarray(NULL, num_glyphs, sizeof(*glyphs));
     num_glyphs = 0;
     for (uint32_t i = 0; i < len; ) {
         U8_NEXT(utf8, i, len, uc);
         glyphs[num_glyphs++] = uc;
     }

    text_stream = xcb_render_util_composite_text_stream(font.glyphset, num_glyphs,
            0);

    if (rect != NULL) {
        xcb_render_fill_rectangles(g_dpy, XCB_RENDER_PICT_OP_OVER, picture,
            background_color, 1, rect);
    }
    xcb_render_util_glyphs_32(text_stream, x, y, num_glyphs, glyphs);
    free(glyphs);

    xcb_render_util_composite_text(g_dpy, XCB_RENDER_PICT_OP_OVER,
        foreground, picture, 0, 0, 0, text_stream);

    xcb_render_util_composite_text_free(text_stream);
    xcb_render_free_picture(g_dpy, picture);
}

/* Measure a text that has no new lines. */
void measure_text(const FcChar8 *utf8, uint32_t len, struct text_measure *tm)
{
    uint32_t uc;

    tm->ascent = font.face->size->metrics.ascender / 64;
    tm->descent = font.face->size->metrics.descender / 64;
    tm->total_width = 0;
    for (uint32_t i = 0; i < len; ) {
        U8_NEXT(utf8, i, len, uc);
        /* TODO: give font multiple faces */
        if (FT_Load_Char(font.face, uc, FT_LOAD_DEFAULT) == 0) {
            tm->total_width += font.face->glyph->advance.x / 64;
        }
    }
}
