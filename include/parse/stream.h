#ifndef PARSE__STREAM_H
#define PARSE__STREAM_H

/**
 * The stream abstracts over getting input.  The input may come from a string
 * or a file.  It gives consistent line endings (\n) and joins lines that have
 * special constructs with a new line followed by any amount of blacks and then
 * a backslash \.  It also skips comments which are lines starting with '#'.
 */

#include "utility/utility.h"
#include "utility/types.h"

/* the input stream object */
typedef struct input_stream {
    /* the path of the file, this is `NULL` if the source is a string */
    utf8_t *file_path;
    /* the current index within the string */
    size_t index;
    /* the length of the the string */
    size_t length;
    /* the string */
    utf8_t string[];
} InputStream;

/* Initialize the internal stream object to parse file at given path.
 *
 * @return NULL if the file could not be opened, otherwise the stream object.
 */
InputStream *create_file_stream(const utf8_t *path);

/* Initialize the internal stream object to parse a given string.
 *
 * @return the allocated stream object.
 */
InputStream *create_string_stream(const utf8_t *string);

/* Destroy a previously allocated input stream object. */
void destroy_stream(_Nullable InputStream *stream);

/* Get the next character from given stream.
 *
 * @return EOF if the end has been reached.
 */
int get_stream_character(InputStream *stream);

/* Get the next character from given stream without advancing to the following
 * character.
 *
 * @return EOF if the end has been reached.
 */
int peek_stream_character(InputStream *stream);

/**
 * NOTE: The below functions are not efficient and should only be used for error
 * output.
 */

/* Get the column and line of @index within the active stream.
 *
 * If @index is out of bounds, @line and @column are set to the last position in
 * the stream.
 */
void get_stream_position(const InputStream *stream, size_t index,
        unsigned *line, unsigned *column);

/* Get the line within the current stream.
 *
 * @length will hold the length of the line.
 */
const char *get_stream_line(const InputStream *stream, unsigned line,
        unsigned *length);

#endif
