#include <inttypes.h>
#include <string.h>

#include <xcb/xcb_image.h>

#include "cursor.h"
#include "fensterchef.h"
#include "log.h"
#include "render.h"
#include "resources.h"
#include "utility.h"
#include "x11_management.h"

/* data to manage cursors */
struct xcursor xcursor_settings;

/* all cached cursors */
static xcb_cursor_t cached_cursors[XCURSOR_MAX];

/* translation of core cursor to string */
const char *xcursor_core_strings[XCURSOR_MAX] = {
#define X(constant, string) [constant] = string,
    DEFINE_ALL_XCURSORS
#undef X
};

/* translation of error to string */
const char *xcursor_error_strings[XCURSOR_ERROR_MAX] = {
#define X(code, string) [code] = string,
    DEFINE_ALL_XCURSOR_ERRORS
#undef X
};

/* Translate a string to a cursor constant. */
core_cursor_t string_to_cursor(const char *string)
{
    for (core_cursor_t i = 0; i < XCURSOR_MAX; i++) {
        if (strcasecmp(xcursor_core_strings[i], string) == 0) {
            return i;
        }
    }
    return XCURSOR_MAX;
}

/* Loosely convert a string to a boolean. */
bool xcursor_string_to_boolean(const char *string)
{
    if (string[0] == 'f' || string[0] == 'n' || string[0] == '0' ||
            (string[0] == 'o' && string[1] == 'f')) {
        return false;
    }
    return true;
}

/* Set the xcursor data settings to the default. */
void set_default_xcursor_settings(void)
{
    /* versions older than 0.8 do not support animated cursors */
    if (render_version.major == 0 && render_version.minor < 8) {
        xcursor_settings.animated = false;
    } else {
        xcursor_settings.animated = true;
    }

    /* versions older than 0.5 do not support the CreateCursor requests */
    if (render_version.major == 0 && render_version.minor < 5) {
        xcursor_settings.has_create_cursor = false;
    } else {
        xcursor_settings.has_create_cursor = true;
    }

    free(xcursor_settings.path);
    xcursor_settings.path = xstrdup(XCURSOR_DEFAULT_PATH);
    xcursor_settings.resized = false;
    xcursor_settings.size = 0;
    xcursor_settings.theme = NULL;
    xcursor_settings.theme_core = false;
}

/* Overwrite values set through the X resources with the ones set as environment
 * variables, they take precedence.
 */
void overwrite_xcursor_settings(void)
{
    const char *variable;

    variable = getenv("XCURSOR_ANIM");
    if (variable != NULL) {
        xcursor_settings.animated = xcursor_string_to_boolean(variable);
    }

    variable = getenv("XCURSOR_CORE");
    if (variable != NULL) {
        xcursor_settings.has_create_cursor =
            xcursor_string_to_boolean(variable);
    }

    variable = getenv("XCURSOR_PATH");
    if (variable != NULL) {
        free(xcursor_settings.path);
        xcursor_settings.path = xstrdup(variable);
    }

    variable = getenv("XCURSOR_RESIZED");
    if (variable != NULL) {
        xcursor_settings.resized = xcursor_string_to_boolean(variable);
    }

    variable = getenv("XCURSOR_SIZE");
    if (variable != NULL) {
        xcursor_settings.size = strtol(variable, NULL, 10);
    }

    variable = getenv("XCURSOR_THEME");
    if (variable != NULL) {
        free(xcursor_settings.theme);
        xcursor_settings.theme = xstrdup(variable);
    }

    variable = getenv("XCURSOR_THEME_CORE");
    if (variable != NULL) {
        xcursor_settings.theme_core = xcursor_string_to_boolean(variable);
    }
}

/* Read a 32 byte large integer from @file in little endianness format. */
static bool read32(FILE *file, uint32_t *destination)
{
    uint8_t bytes[4];

    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
        return false;
    }
    *destination = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) |
        bytes[0];
    return true;
}

/* Skip over a comment chunk. */
xcursor_error_t skip_comment_chunk(FILE *file, uint32_t subtype)
{
    struct xcursor_comment_header header;

    /* read the length */
    if (!read32(file, &header.length)) {
        return XCURSOR_ERROR_INVALID_COMMENT_CHUNK;
    }

    /* the sub type should be one of:
     * 1 (COPYRIGHT)
     * 2 (LICENSE)
     * 3 (OTHER)
     */
    if (subtype < 1 || subtype > 3) {
        return XCURSOR_ERROR_INVALID_COMMENT_SUBTYPE;
    }

    /* skip the comment string */
    fseek(file, header.length, SEEK_CUR);

    return XCURSOR_SUCCESS;
}

/* Load an image chunk from @file. */
xcursor_error_t load_cursor_image(FILE *file, struct xcursor_image *image)
{
    /* read image size */
    if (!read32(file, &image->header.width) ||
            !read32(file, &image->header.height)) {
        return XCURSOR_ERROR_INVALID_IMAGE_CHUNK;
    }

    /* check the image bounds */
    if (image->header.width >= XCURSOR_MAX_IMAGE_SIZE ||
            image->header.height >= XCURSOR_MAX_IMAGE_SIZE) {
        LOG("%u, %u\n", image->header.width, image->header.height);
        return XCURSOR_ERROR_IMAGE_TOO_LARGE;
    }

    /* read the hotspot */
    if (!read32(file, &image->header.xhot) ||
            !read32(file, &image->header.yhot)) {
        return XCURSOR_ERROR_INVALID_IMAGE_CHUNK;
    }

    /* confirm that the hotspot is in bound */
    if (image->header.xhot >= image->header.width ||
            image->header.yhot >= image->header.height) {
        return XCURSOR_ERROR_INVALID_IMAGE_HOTSPOT;
    }

    /* read the delay */
    if (!read32(file, &image->header.delay)) {
        return XCURSOR_ERROR_INVALID_IMAGE_CHUNK;
    }

    /* read the pixels */
    const uint32_t length = image->header.width * image->header.height;
    image->pixels = xreallocarray(NULL, length, sizeof(uint32_t));
    for (uint32_t i = 0; i < length; i++) {
        if (!read32(file, &image->pixels[i])) {
            free(image->pixels);
            return XCURSOR_ERROR_MISSING_IMAGE_DATA;
        }
    }

    return XCURSOR_SUCCESS;
}

/* Read a file as cursor file. */
xcursor_error_t load_cursor_file(FILE *file, struct xcursor_file *xcursor)
{
    xcursor_error_t error = XCURSOR_SUCCESS;

    struct xcursor_header header;

    uint32_t number_of_images = 0;

    memset(xcursor, 0, sizeof(*xcursor));

    /* read the header */
    if (!read32(file, &header.magic) ||
            !read32(file, &header.header) ||
            !read32(file, &header.version) ||
            !read32(file, &header.number_of_entries)) {
        return XCURSOR_ERROR_INVALID_FILE;
    }

    /* confirm the magic */
    if (header.magic != XCURSOR_MAGIC) {
        return XCURSOR_ERROR_INVALID_FILE;
    }

    /* TODO: confirm the version, I think it should be 0x10000 */

    /* read entries from the table of contents */
    struct xcursor_entry entries[header.number_of_entries];
    for (uint32_t i = 0; i < header.number_of_entries; i++) {
        struct xcursor_entry *const entry = &entries[i];
        if (!read32(file, &entry->type) ||
                !read32(file, &entry->subtype) ||
                !read32(file, &entry->position)) {
            error = XCURSOR_ERROR_MISSING_TABLE_OF_CONTENTS;
            break;
        }
        number_of_images++;
    }

    if (number_of_images == 0) {
        error = XCURSOR_ERROR_FILE_WITHOUT_IMAGES;
    }

    /* allocate enough space to hold onto all images */
    RESIZE(xcursor->images, number_of_images);

    /* read the chunks */
    for (uint32_t i = 0;
            error == XCURSOR_SUCCESS && i < header.number_of_entries;
            i++) {
        struct xcursor_chunk_header chunk_header;

        /* seek to the chunk */
        if (fseek(file, entries[i].position, SEEK_SET) == EOF) {
            error = XCURSOR_ERROR_MISSING_CHUNK;
            break;
        }

        /* read the chunk header */
        if (!read32(file, &chunk_header.header) ||
                !read32(file, &chunk_header.type) ||
                !read32(file, &chunk_header.subtype)) {
            error = XCURSOR_ERROR_INVALID_CHUNK_HEADER;
            break;
        }

        /* these must match says the specification */
        if (chunk_header.type != entries[i].type ||
                chunk_header.subtype != entries[i].subtype) {
            error = XCURSOR_ERROR_INVALID_CHUNK_HEADER;
            break;
        }

        /* confirm the version */
        if (!read32(file, &chunk_header.version)) {
            error = XCURSOR_ERROR_INVALID_CHUNK_HEADER;
            break;
        }
        if (chunk_header.version != XCURSOR_CHUNK_VERSION) {
            error = XCURSOR_ERROR_UNSUPPORTED_CHUNK_VERSION;
            break;
        }

        switch (chunk_header.type) {
        /* ignore comment chunks */
        case XCURSOR_COMMENT_TYPE:
            error = skip_comment_chunk(file, chunk_header.subtype);
            break;

        /* load image chunks */
        case XCURSOR_IMAGE_TYPE: {
            struct xcursor_image *image;

            image = &xcursor->images[xcursor->number_of_images];
            image->size = chunk_header.subtype;
            error = load_cursor_image(file, image);
            if (error != XCURSOR_SUCCESS) {
                break;
            }
            xcursor->number_of_images++;
            break;
        }
        }
    }

    if (error != XCURSOR_SUCCESS) {
        clear_cursor_file(xcursor);
    }
    return error;
}

/* Clear the resources occupied by @xcursor. */
void clear_cursor_file(struct xcursor_file *xcursor)
{
    for (uint32_t i = 0; i < xcursor->number_of_images; i++) {
        free(xcursor->images[i].pixels);
    }
    free(xcursor->images);
}

/* libxcursor uses these exact macros for whitespace/separator checking */
#define is_xcursor_white(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')
#define is_xcursor_separator(c) ((c) == ';' || (c) == ',')

/* Themes can define other themes they inherit from.
 *
 * This looks for such definitions within @file_path and returns the value of
 * this which is usually a list of paths.
 */
static char *get_theme_inherits(const char *file_path)
{
    char line[8192];
    char *result = NULL;
    FILE *file;
    char *name, *value;

    file = fopen(file_path, "r");
    if (file == NULL) {
        return NULL;
    }

    /* go through all lines and find the first Inherits variable; its value is a
     * list separated by : or ;
     *
     * the syntax of a line in this file is either:
     * [X]
     * or:
     * <name> = <value>
     */
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "Inherits", 8) != 0) {
            continue;
        }

        name = line + strlen("Inherits");

        while (name[0] == ' ') {
            name++;
        }

        if (name[0] != '=') {
            continue;
        }

        name++;
        while (name[0] == ' ') {
            name++;
        }

        /* allocate enough for the resulting value and then sanitize it by
         * removing any space or duplicated/leading/trailing separators
         */
        result = xmalloc(strlen(name) + 1);
        value = result;
        while (name[0] != '\0') {
            /* skip space and separators */
            while (is_xcursor_separator(name[0]) ||
                    is_xcursor_white(name[0])) {
                name++;
            }

            if (name[0] == '\0') {
                break;
            }

            if (value != result) {
                value[0] = ':';
                value++;
            }
            while (name[0] != '\0' && !is_xcursor_white(name[0]) &&
                    !is_xcursor_separator(name[0])) {
                value[0] = name[0];
                value++;
                name++;
            }
        }
        value[0] = '\0';
        value++;
    }

    fclose(file);
    return result;
}

/* how many times to go deeper into inherited theme files */
#define XCURSOR_MAX_INHERITS_DEPTH 128

/* Find the cursor named @name within theme named @theme.
 *
 * Call this with @inherit_depth set to 0.
 */
static FILE *open_cursor_file(const char *theme, const char *name,
        uint32_t inherit_depth, char **pointer_path)
{
    FILE *file = NULL;
    char *inherits = NULL;
    const char *path, *separator;

    /* go over all paths and try to find the theme and cursor name */
    for (path = xcursor_settings.path;
            path != NULL && file == NULL;
            path = (separator != NULL ? separator + 1 : NULL)) {
        char *theme_directory;
        char *full;

        separator = strchr(path, ':');

        const int path_length = separator ? (size_t) (separator - path) :
            strlen(path);
        if (path[0] == '~' && path[1] == '/') {
            theme_directory = xasprintf("%s/%.*s/%s", fensterchef_home,
                    path_length - 2, &path[2], theme);
        } else {
            theme_directory = xasprintf("%.*s/%s", path_length, path, theme);
        }
        full = xasprintf("%s/cursors/%s", theme_directory, name);
        file = fopen(full, "r");
        if (file != NULL) {
            *pointer_path = full;
        } else {
            free(full);
        }

        /* themes can inherit data from other themes */
        if (file == NULL && inherits == NULL) {
            full = xasprintf("%s/index.theme", theme_directory);
            inherits = get_theme_inherits(full);
            free(full);
        }
        free(theme_directory);
    }

    /* avoid recursing through theme files if there is a cycle */
    if (inherit_depth < XCURSOR_MAX_INHERITS_DEPTH) {
        /* check the inherited files */
        for (path = inherits;
                path != NULL && file == NULL;
                separator = strchr(path, ':'),
                    path = separator == NULL ? NULL : separator + 1) {
            file = open_cursor_file(path, name, inherit_depth + 1,
                    pointer_path);
        }
    }

    free(inherits);

    return file;
}

/* Upscale given image to given size @new_size. */
static void scale_cursor_image(struct xcursor_image *image, uint32_t new_size)
{
    uint32_t new_width, new_height;
    uint32_t *new_pixels;

    new_width = (uint32_t) (image->header.width * new_size / image->size);
    new_height = (uint32_t) (image->header.height * new_size / image->size);

    new_width = MAX(new_width, 1);
    new_height = MAX(new_height, 1);
    new_width = MIN(new_width, XCURSOR_MAX_IMAGE_SIZE);
    new_height = MIN(new_height, XCURSOR_MAX_IMAGE_SIZE);

    new_pixels = xreallocarray(NULL, new_width * new_height,
            sizeof(*new_pixels));

    /* very simple up/downscaling:
     * if the new size is bigger, pixels are filled by copying the neighbor
     * if the new size is smaller, pixels are skipped
     */
    for (uint32_t dest_y = 0; dest_y < new_height; dest_y++)
    {
        const uint32_t src_y = dest_y * image->size / new_size;
        uint32_t *const src_row = image->pixels + src_y * image->header.width;
        uint32_t *const dest_row = new_pixels + dest_y * new_width;
        for (uint32_t dest_x = 0; dest_x < new_width; dest_x++)
        {
            const uint32_t src_x = dest_x * image->size / new_size;
            dest_row[dest_x] = src_row[src_x];
        }
    }

    image->header.width = new_width;
    image->header.height = new_height;
    image->header.xhot = image->header.xhot * new_size / image->size;
    image->header.yhot = image->header.yhot * new_size / image->size;
    image->size = new_size;
    free(image->pixels);
    image->pixels = new_pixels;
}

/* Create a cursor from an image loaded from an Xcursor file. */
static xcb_cursor_t create_cursor_from_image(const struct xcursor_image *image)
{
    xcb_cursor_t cursor_id = XCB_NONE;

    xcb_image_t *xcb_image;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    xcb_render_picture_t picture;

    cursor_id = xcb_generate_id(connection);

    pixmap = xcb_generate_id(connection);
    gc = xcb_generate_id(connection);
    picture = xcb_generate_id(connection);

    xcb_image = xcb_image_create_native(connection,
            image->header.width, image->header.height,
            XCB_IMAGE_FORMAT_Z_PIXMAP, 32,
            NULL, image->header.width * image->header.height *
                sizeof(*image->pixels), (uint8_t*) image->pixels);

    xcb_create_pixmap(connection, 32, pixmap, screen->root,
            image->header.width, image->header.height);
    xcb_create_gc(connection, gc, pixmap, 0, NULL);
    xcb_image_put(connection, pixmap, gc, xcb_image, 0, 0, 0);
    xcb_free_gc(connection, gc);

    xcb_render_create_picture(connection, picture, pixmap,
            get_picture_format(32), 0, NULL);
    xcb_free_pixmap(connection, pixmap);

    xcb_render_create_cursor(connection, cursor_id,
            picture, image->header.xhot, image->header.yhot);

    xcb_render_free_picture(connection, picture);
    xcb_image_destroy(xcb_image);

    return cursor_id;
}

/* Load the cursor with given name using the user's preferred style. */
xcb_cursor_t load_cursor(core_cursor_t cursor)
{
    FILE *file = NULL;
    char *path;

    xcb_cursor_t cursor_id;

    struct xcursor_file xcursor;
    xcursor_error_t error;
    uint32_t size = 0;
    uint32_t number_of_images = 0;

    /* check if the cursor is cached */
    if (cached_cursors[cursor] != XCB_NONE) {
        return cached_cursors[cursor];
    }

    const char *const name = xcursor_core_strings[cursor];

    /* if the core font is not preferred, load the preferred theme */
    if (xcursor_settings.has_create_cursor && !xcursor_settings.theme_core) {
        /* load the font given by the user */
        if (xcursor_settings.theme != NULL) {
            file = open_cursor_file(xcursor_settings.theme, name, 0, &path);
            if (file == NULL) {
                LOG("could not find %s in %s\n", name, xcursor_settings.theme);
            }
        }

        /* load the default font */
        if (file == NULL) {
            file = open_cursor_file("default", name, 0, &path);
            if (file == NULL) {
                LOG("could not find %s in %s\n", name, "default");
            }
        }
    }

    if (file == NULL) {
        LOG("setting cursor %s using the core cursor font\n", name);

        if (xcursor_settings.cursor_font == XCB_NONE) {
            xcursor_settings.cursor_font = xcb_generate_id(connection);
            /* open the "cursor" font, this is a special font holding the
             * default core cursors
             */
            xcb_open_font(connection, xcursor_settings.cursor_font,
                    strlen("cursor"), "cursor");
        }

        cursor_id = xcb_generate_id(connection);
        /* create a black and white cursor */
        xcb_create_glyph_cursor(connection, cursor_id,
                xcursor_settings.cursor_font, xcursor_settings.cursor_font,
                cursor * 2, cursor * 2 + 1,
                0, 0, 0, 0xffff, 0xffff, 0xffff);
        return cursor_id;
    }

    LOG("loading cursor %s from %s\n", name, path);

    error = load_cursor_file(file, &xcursor);
    if (error != XCURSOR_SUCCESS) {
        const long file_position = ftell(file);
        LOG_ERROR("error reading cursor file %s: %s"
                    "(approximately at: %ld (%#lx))\n",
                path, xcursor_error_strings[error],
                file_position, file_position);
        free(path);
        fclose(file);
        return XCB_NONE;
    }
    free(path);
    fclose(file);

    /* if no size is given, use the dpi to make the cursor 16 "points" tall */
    const uint32_t target_size = xcursor_settings.size == 0 ?
        resources.dpi * 16 / 72 : xcursor_settings.size;

    /* get the best fitting size and count all images that have this size */
    for (uint32_t i = 0; i < xcursor.number_of_images; i++) {
        const uint32_t image_size = xcursor.images[i].size;
        if (size == 0 || ABSOLUTE_DIFFERENCE(image_size, target_size) <
                ABSOLUTE_DIFFERENCE(size, target_size)) {
            size = image_size;
            number_of_images = 0;
        }
        if (image_size == size) {
            number_of_images++;
        }
    }

    LOG("cursor consists of %u equally sized images\n", number_of_images);

    if (!xcursor_settings.animated && number_of_images > 1) {
        number_of_images = 1;
        LOG("but they were trimmed to %u because animated cursors are off\n",
                number_of_images);
    }

    if (target_size != size && xcursor_settings.resized) {
        LOG("artificial sizing will be done to get from size %" PRIu32 " to %"
                    PRIu32 "\n",
                size, target_size);
    }

    /* create an animated cursor or not if it is just a single image */
    xcb_render_animcursorelt_t cursors[number_of_images];

    for (uint32_t i = 0, j = 0; j < number_of_images; i++) {
        struct xcursor_image *const image = &xcursor.images[i];
        if (image->size != size) {
            continue;
        }

        /* scale the image if needed and wanted */
        if (image->size != target_size && xcursor_settings.resized) {
            scale_cursor_image(image, target_size);
        }

        cursors[j].cursor = create_cursor_from_image(image);
        cursors[j].delay = image->header.delay;
        j++;
    }

    if (number_of_images > 1) {
        cursor_id = xcb_generate_id(connection);
        xcb_render_create_anim_cursor(connection, cursor_id,
                number_of_images, cursors);
        for (uint32_t i = 0; i < number_of_images; i++) {
            xcb_free_cursor(connection, cursors[i].cursor);
        }
    } else {
        cursor_id = cursors[0].cursor;
    }

    clear_cursor_file(&xcursor);

    cached_cursors[cursor] = cursor_id;

    return cursor_id;
}

/* Clear all cached cursors. */
void clear_cursor_cache(void)
{
    for (core_cursor_t i = 0; i < XCURSOR_MAX; i++) {
        if (cached_cursors[i] != XCB_NONE) {
            xcb_free_cursor(connection, cached_cursors[i]);
            cached_cursors[i] = XCB_NONE;
        }
    }
}
