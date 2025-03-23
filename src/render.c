#include <inttypes.h>

#include "configuration.h"
#include "log.h"
#include "render.h"
#include "utility.h"
#include "x11_management.h"

/* TODO: how do we make colored emojis work?
 * I tried to but color formats other than 8 bit colors do not work
 *
 * TODO: and how to get fonts with fixed size to scale?
 */

/* the version of the underlying X renderer */
struct xrender_version Render_version;

/* the render formats for pictures */
static xcb_render_query_pict_formats_reply_t *picture_formats;

/* Get the format of a visual. */
xcb_render_pictformat_t find_visual_format(xcb_visualid_t visual)
{
    xcb_render_pictscreen_iterator_t screens;
    xcb_render_pictdepth_iterator_t depths;
    xcb_render_pictvisual_iterator_t visuals;

    if (picture_formats == NULL) {
        return XCB_NONE;
    }

    /* for each screen */
    screens = xcb_render_query_pict_formats_screens_iterator(picture_formats);
    for (; screens.rem > 0; xcb_render_pictscreen_next(&screens)) {
        /* for each screen's supported depths */
        depths = xcb_render_pictscreen_depths_iterator(screens.data);
	    for (; depths.rem > 0; xcb_render_pictdepth_next(&depths)) {
            /* for each supported depth's visuals  */
            visuals = xcb_render_pictdepth_visuals_iterator(depths.data);
	        for (; visuals.rem > 0; xcb_render_pictvisual_next(&visuals)) {
		        if (visuals.data->visual == visual) {
		            return visuals.data->format;
		        }
		    }
		}
	}
    return XCB_NONE;
}

/* Get a good picture format for given depth. */
xcb_render_pictformat_t get_picture_format(uint8_t depth)
{
    xcb_render_pictforminfo_iterator_t i;

    /* iterate over all picture formats and pick the one with needed depth */
    for (i = xcb_render_query_pict_formats_formats_iterator(picture_formats);
            i.rem > 0; xcb_render_pictforminfo_next(&i)) {
        /* the direct type is for storing colors per pixel without a palette */
        if (i.data->depth == depth &&
                i.data->type == XCB_RENDER_PICT_TYPE_DIRECT) {
            return i.data->id;
        }
    }
    return 0;
}

/* Initialize the graphical stock objects that can be used for rendering. */
int initialize_renderer(void)
{
    const xcb_query_extension_reply_t *extension;
    xcb_render_query_version_cookie_t version_cookie;
    xcb_render_query_pict_formats_cookie_t formats_cookie;
    xcb_render_query_version_reply_t *version;

    extension = xcb_get_extension_data(connection, &xcb_render_id);
    if (extension != NULL && extension->present) {
        version_cookie = xcb_render_query_version(connection,
                XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);
        formats_cookie = xcb_render_query_pict_formats(connection);

        version = xcb_render_query_version_reply(connection, version_cookie,
                NULL);
        if (version != NULL) {
            Render_version.major = version->major_version;
            Render_version.minor = version->minor_version;
            free(version);
        }

        /* the picture formats are the possible ways colors can be represented,
         * for example ARGB, 8 bit colors etc.
         */
        picture_formats = xcb_render_query_pict_formats_reply(connection,
                formats_cookie, NULL);
    }

    /* initialize both font drawing systems */
    initialize_legacy_font_drawing();
    initialize_modern_font_drawing();

    /* even if no rendering works at all, this shall not stop fensterchef from
     * starting up
     *
     * it might cause log messages to omit errors in the future, but they can be
     * ignored
     */
    return OK;
}

/* Measure a text that has no new lines. */
void measure_text(const utf8_t *utf8, uint32_t length,
        struct text_measure *measure)
{
    if (configuration.font.use_core_font || !has_modern_font_drawing()) {
        measure_text_legacy(utf8, length, measure);
    } else {
        measure_text_modern(utf8, length, measure);
    }
}

/* Draw text to a given drawable using the current font. */
void draw_text(xcb_drawable_t drawable, const utf8_t *utf8, uint32_t length,
        uint32_t background_color, const xcb_rectangle_t *rectangle,
        uint32_t foreground_color, int32_t x, int32_t y)
{
    if (configuration.font.use_core_font || !has_modern_font_drawing()) {
        draw_text_legacy(drawable, utf8, length, background_color,
                rectangle, foreground_color, x, y);
    } else {
        draw_text_modern(drawable, utf8, length, background_color,
                rectangle, foreground_color, x, y);
    }
}
