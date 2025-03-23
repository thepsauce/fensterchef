#include "render.h"
#include "x11_management.h"

/* graphics context used for rendering legacy font */
static xcb_gcontext_t Render_font_gc;

/* legacy font */
static xcb_font_t Render_font;

/* Initialize the legacy font drawing. */
void initialize_legacy_font_drawing(void)
{
    const char *core_font = "*";

    /* open the fallback font */
    Render_font = xcb_generate_id(connection);
    xcb_open_font(connection, Render_font, strlen(core_font), core_font);

    /* create a graphics context */
    Render_font_gc = xcb_generate_id(connection);
    general_values[0] = Render_font;
    xcb_create_gc(connection, Render_font_gc, screen->root,
            XCB_GC_FONT, general_values);
}

/* Convert a utf8 encoded string to a ucs-2 encoded string. */
static xcb_char2b_t *utf8_to_ucs2(const utf8_t *utf8, uint32_t length,
        uint32_t *ucs_length)
{
    xcb_char2b_t *ucs = NULL;
    uint32_t capacity = 128;
    uint32_t index = 0;
    uint32_t glyph;

    RESIZE(ucs, capacity);
    for (uint32_t i = 0; i < length; ) {
        U8_NEXT(utf8, i, length, glyph);

        if (index == capacity) {
            capacity *= 2;
            RESIZE(ucs, capacity);
        }

        if (glyph > UINT16_MAX) {
            ucs[index].byte1 = 0;
            ucs[index].byte2 = 0;
        } else {
            ucs[index].byte1 = glyph >> 8;
            ucs[index].byte2 = glyph;
        }
        index++;
    }

    *ucs_length = index;
    return ucs;
}

/* Draw text using the X core font. */
void draw_text_legacy(xcb_drawable_t drawable, const utf8_t *utf8,
        uint32_t length, uint32_t background_color,
        const xcb_rectangle_t *rectangle, uint32_t foreground_color,
        int32_t x, int32_t y)
{
    xcb_char2b_t *ucs;
    xcb_query_text_extents_reply_t *extents;

    (void) rectangle;

    if (length == 0) {
        return;
    }

    general_values[0] = foreground_color;
    general_values[1] = background_color;
    xcb_change_gc(connection, Render_font_gc,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, general_values);

    ucs = utf8_to_ucs2(utf8, length, &length);

    /* if there are more characters than UINT8_MAX, then we must draw them in
     * chunks because `xcb_image_draw_text_16()` is limited to that many
     * characters
     */
    const uint32_t segments = (length - 1) / UINT8_MAX;
    xcb_query_text_extents_cookie_t extents_cookies[segments];

    for (uint32_t i = 0, j = UINT8_MAX; i < segments; i++) {
        extents_cookies[i] = xcb_query_text_extents(connection, Render_font,
            MIN(length - j, UINT8_MAX), &ucs[j]);
        j += UINT8_MAX;
    }

    for (uint32_t i = 0, j = 0; i <= segments; i++) {
        if (i > 0) {
            extents = xcb_query_text_extents_reply(connection,
                    extents_cookies[i], NULL);
            if (extents != NULL) {
                x += extents->overall_width;
                free(extents);
            }
        }
        xcb_image_text_16(connection, MIN(length - j, UINT8_MAX), drawable,
                Render_font_gc, x, y, &ucs[j]);
        j += UINT8_MAX;
    }

    free(ucs);
}

/* Measure a text that has no new lines. */
void measure_text_legacy(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure)
{
    xcb_query_text_extents_cookie_t extents_cookie;
    xcb_query_text_extents_reply_t *extents;

    xcb_char2b_t *const ucs = utf8_to_ucs2(utf8, length, &length);
    extents_cookie = xcb_query_text_extents(connection, Render_font,
            length, ucs);
    free(ucs);

    extents = xcb_query_text_extents_reply(connection, extents_cookie,
            NULL);
    if (extents != NULL) {
        measure->ascent = extents->overall_ascent;
        measure->descent = extents->overall_descent;
        measure->total_width = extents->overall_width;
        measure->total_height = measure->ascent - measure->descent;
        free(extents);
    }
}
