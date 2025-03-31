#ifndef CURSOR_H
#define CURSOR_H

/**
 * Just like xcb-ewmh, xcb-cursor is shit which is why we brew up our own
 * implementation.
 *
 * What is NOT supported compared to libxcursor:
 * The XCURSOR_DITHER variable.
 * Dithering is applied when the core theme is used but I will not bother using
 * something that nobody even knows exists. And colored/animated cursors are
 * probably more trendy.
 *
 * Writing cursor files.
 * This is just not something we need to do in this project.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <xcb/xcb.h>

/* the default paths to look for cursor files */
#define XCURSOR_DEFAULT_PATH \
    "~/.icons:" \
    "~/.local/share/icons:" \
    "/usr/share/icons:" \
    "/usr/share/pixmaps"

/* expands to all X core cursor names */
#define DEFINE_ALL_XCURSORS \
    X(XCURSOR_X_CURSOR, "X_cursor") \
    X(XCURSOR_ARROW, "arrow") \
    X(XCURSOR_BASED_ARROW_DOWN, "based_arrow_down") \
    X(XCURSOR_BASED_ARROW_UP, "based_arrow_up") \
    X(XCURSOR_BOAT, "boat") \
    X(XCURSOR_BOGOSITY, "bogosity") \
    X(XCURSOR_BOTTOM_LEFT_CORNER, "bottom_left_corner") \
    X(XCURSOR_BOTTOM_RIGHT_CORNER, "bottom_right_corner") \
    X(XCURSOR_BOTTOM_SIDE, "bottom_side") \
    X(XCURSOR_BOTTOM_TEE, "bottom_tee") \
    X(XCURSOR_BOX_SPIRAL, "box_spiral") \
    X(XCURSOR_CENTER_PTR, "center_ptr") \
    X(XCURSOR_CIRCLE, "circle") \
    X(XCURSOR_CLOCK, "clock") \
    X(XCURSOR_COFFEE_MUG, "coffee_mug") \
    X(XCURSOR_CROSS, "cross") \
    X(XCURSOR_CROSS_REVERSE, "cross_reverse") \
    X(XCURSOR_CROSSHAIR, "crosshair") \
    X(XCURSOR_DIAMOND_CROSS, "diamond_cross") \
    X(XCURSOR_DOT, "dot") \
    X(XCURSOR_DOTBOX, "dotbox") \
    X(XCURSOR_DOUBLE_ARROW, "double_arrow") \
    X(XCURSOR_DRAFT_LARGE, "draft_large") \
    X(XCURSOR_DRAFT_SMALL, "draft_small") \
    X(XCURSOR_DRAPED_BOX, "draped_box") \
    X(XCURSOR_EXCHANGE, "exchange") \
    X(XCURSOR_FLEUR, "fleur") \
    X(XCURSOR_GOBBLER, "gobbler") \
    X(XCURSOR_GUMBY, "gumby") \
    X(XCURSOR_HAND1, "hand1") \
    X(XCURSOR_HAND2, "hand2") \
    X(XCURSOR_HEART, "heart") \
    X(XCURSOR_ICON, "icon") \
    X(XCURSOR_IRON_CROSS, "iron_cross") \
    X(XCURSOR_LEFT_PTR, "left_ptr") \
    X(XCURSOR_LEFT_SIDE, "left_side") \
    X(XCURSOR_LEFT_TEE, "left_tee") \
    X(XCURSOR_LEFTBUTTON, "leftbutton") \
    X(XCURSOR_LL_ANGLE, "ll_angle") \
    X(XCURSOR_LR_ANGLE, "lr_angle") \
    X(XCURSOR_MAN, "man") \
    X(XCURSOR_MIDDLEBUTTON, "middlebutton") \
    X(XCURSOR_MOUSE, "mouse") \
    X(XCURSOR_PENCIL, "pencil") \
    X(XCURSOR_PIRATE, "pirate") \
    X(XCURSOR_PLUS, "plus") \
    X(XCURSOR_QUESTION_ARROW, "question_arrow") \
    X(XCURSOR_RIGHT_PTR, "right_ptr") \
    X(XCURSOR_RIGHT_SIDE, "right_side") \
    X(XCURSOR_RIGHT_TEE, "right_tee") \
    X(XCURSOR_RIGHTBUTTON, "rightbutton") \
    X(XCURSOR_RTL_LOGO, "rtl_logo") \
    X(XCURSOR_SAILBOAT, "sailboat") \
    X(XCURSOR_SB_DOWN_ARROW, "sb_down_arrow") \
    X(XCURSOR_SB_H_DOUBLE_ARROW, "sb_h_double_arrow") \
    X(XCURSOR_SB_LEFT_ARROW, "sb_left_arrow") \
    X(XCURSOR_SB_RIGHT_ARROW, "sb_right_arrow") \
    X(XCURSOR_SB_UP_ARROW, "sb_up_arrow") \
    X(XCURSOR_SB_V_DOUBLE_ARROW, "sb_v_double_arrow") \
    X(XCURSOR_SHUTTLE, "shuttle") \
    X(XCURSOR_SIZING, "sizing") \
    X(XCURSOR_SPIDER, "spider") \
    X(XCURSOR_SPRAYCAN, "spraycan") \
    X(XCURSOR_STAR, "star") \
    X(XCURSOR_TARGET, "target") \
    X(XCURSOR_TCROSS, "tcross") \
    X(XCURSOR_TOP_LEFT_ARROW, "top_left_arrow") \
    X(XCURSOR_TOP_LEFT_CORNER, "top_left_corner") \
    X(XCURSOR_TOP_RIGHT_CORNER, "top_right_corner") \
    X(XCURSOR_TOP_SIDE, "top_side") \
    X(XCURSOR_TOP_TEE, "top_tee") \
    X(XCURSOR_TREK, "trek") \
    X(XCURSOR_UL_ANGLE, "ul_angle") \
    X(XCURSOR_UMBRELLA, "umbrella") \
    X(XCURSOR_UR_ANGLE, "ur_angle") \
    X(XCURSOR_WATCH, "watch") \
    X(XCURSOR_XTERM, "xterm")

typedef enum {
#define X(constant, string) constant,
    DEFINE_ALL_XCURSORS

    /* indicator for the the number of cursors */
    XCURSOR_MAX
#undef X
} core_cursor_t;

/* translation of core cursor to string, +1 is for technical reasons */
extern const char *xcursor_core_strings[XCURSOR_MAX + 1];

/* Translate a string to a cursor constant. */
core_cursor_t string_to_cursor(const char *string);

/* data to manage cursors */
extern struct xcursor {
    /* if animated cursors are enabled */
    bool animated;
    /* if the renderer supports the CreateCursor request */
    bool has_create_cursor;
    /* colon separated list of paths */
    char *path;
    /* if the cursor should be resized */
    bool resized;
    /* preferred size of the cursor */
    uint32_t size;
    /* name of the cursor theme */
    char *theme;
    /* if the core theme should be used */
    bool theme_core;
    /* the basic builtin X cursor font */
    xcb_font_t cursor_font;
} xcursor_settings;

/* Loosely convert a string to a boolean.
 *
 * Strings starting with any of these are considered false:
 * "f", "n", "0", "of".
 * They represent: "false", "no", "0", "off" respectively.
 *
 * Everthing else is considered true.
 */
bool xcursor_string_to_boolean(const char *string);

/* Set the Xcursor data settings to the default. */
void set_default_xcursor_settings(void);

/* Overwrite values set through the X resources with the ones set as environment
 * variables, they take precedence over X resource entries.
 */
void overwrite_xcursor_settings(void);

/**
 * The Xcursor file format is quite simple and below is the setup to parse such
 * file. Please look up the Xcursor manual for more details.
 */

/* magic number to indicate a cursor file, this is actually "Xcur" in reverse */
#define XCURSOR_MAGIC 0x72756358

/* the version number the file should have */
#define XCURSOR_VERSION 1

/* type of a comment chunk */
#define XCURSOR_COMMENT_TYPE 0xfffe0001

/* type of an image chunk */
#define XCURSOR_IMAGE_TYPE 0xfffd0002

/* the version number a chunk should have */
#define XCURSOR_CHUNK_VERSION 1

/* maximum size of the image width or height */
#define XCURSOR_MAX_IMAGE_SIZE 0x7fff

/* header of the cursor file */
struct xcursor_header {
    /* the magic of the file, must match `XCURSOR_MAGIC` */
    uint32_t magic;
    /* number of bytes in the header */
    uint32_t header;
    /* version of the file */
    uint32_t version;
    /* number of table of contents entries */
    uint32_t number_of_entries;
};

/* table of contents entry */
struct xcursor_entry {
    /* type of the entry */
    uint32_t type;
    /* type specific label;
     * for comments: identifier what kind of comment
     * for images: size of the cursor
     */
    uint32_t subtype;
    /* position of the entry within the file */
    uint32_t position;
};

/* header of a chunk */
struct xcursor_chunk_header {
    /* number of bytes in the header */
    uint32_t header;
    /* same as what the table of contents entry indicates */
    uint32_t type;
    /* same as what the table of contents entry indicates */
    uint32_t subtype;
    /* chunk version number */
    uint32_t version;
};

/* header of a comment chunk */
struct xcursor_comment_header {
    /* length of the comment */
    uint32_t length;
};

/* comment chunk */
struct xcursor_comment {
    /* header of the comment */
    struct xcursor_comment_header header;
    /* the comment itself */
    char *string;
};

/* header of an image chunk */
struct xcursor_image_header {
    /* width of the image */
    uint32_t width;
    /* height of the image */
    uint32_t height;
    /* hotspot position within the image */
    uint32_t xhot;
    uint32_t yhot;
    /* delay between animation frames in milliseconds */
    uint32_t delay;
};

/* image chunk */
struct xcursor_image {
    /* header of the image */
    struct xcursor_image_header header;
    /* cursor size this image represents */
    uint32_t size;
    /* pixels in ARGB format */
    uint32_t *pixels;
};

/* full xcursor file */
struct xcursor_file {
    /* comment chunks (UNUSED) */
    struct xcursor_comment *comments;
    uint32_t number_of_comments;
    /* image chunks */
    struct xcursor_image *images;
    uint32_t number_of_images;
};

/* expands to all possible xcursor file errors */
#define DEFINE_ALL_XCURSOR_ERRORS \
    /* success value */ \
    X(XCURSOR_SUCCESS, "success") \
 \
    X(XCURSOR_ERROR_INVALID_FILE, "invalid file format") \
    X(XCURSOR_ERROR_MISSING_TABLE_OF_CONTENTS, "table of contents is missing or incomplete") \
    X(XCURSOR_ERROR_FILE_WITHOUT_IMAGES, "the cursor file has no image chunks") \
    X(XCURSOR_ERROR_INVALID_CHUNK_HEADER, "invalid chunk header format") \
    X(XCURSOR_ERROR_UNSUPPORTED_CHUNK_VERSION, "chunk version not supported") \
    X(XCURSOR_ERROR_MISSING_CHUNK, "seeking a specific chunk failed") \
 \
    X(XCURSOR_ERROR_INVALID_COMMENT_CHUNK, "invalid comment chunk format") \
    X(XCURSOR_ERROR_INVALID_COMMENT_SUBTYPE, "invalid comment chunk format") \
 \
    X(XCURSOR_ERROR_INVALID_IMAGE_CHUNK, "invalid image chunk format") \
    X(XCURSOR_ERROR_IMAGE_TOO_LARGE, "image exceeds maximum size") \
    X(XCURSOR_ERROR_INVALID_IMAGE_HOTSPOT, "hot spot is out of bounds") \
    X(XCURSOR_ERROR_INVALID_IMAGE_SUBTYPE, "invalid image chunk format") \
    X(XCURSOR_ERROR_MISSING_IMAGE_DATA, "image data is missing or incomplete") \

/* error codes for various things that could go wrong when parsing */
typedef enum xcursor_error {
#define X(code, string) code,
    DEFINE_ALL_XCURSOR_ERRORS

    XCURSOR_ERROR_MAX
#undef X
} xcursor_error_t;

/* translation of error to string */
extern const char *xcursor_error_strings[XCURSOR_ERROR_MAX];

/* Read a file as cursor file.
 *
 * The data in @xcursor is invalid when this function fails.
 *
 * @return XCURSOR_SUCCESS if the file format was correct.
 */
xcursor_error_t load_cursor_file(FILE *file, struct xcursor_file *xcursor);

/* Clear the resources occupied by @file. */
void clear_cursor_file(struct xcursor_file *xcursor);

/* Load the cursor with given name using the user's preferred style. */
xcb_cursor_t load_cursor(core_cursor_t cursor);

/* Clear all cached cursors.
 *
 * This can be used when the theme is changed for example.
 */
void clear_cursor_cache(void);

#endif
