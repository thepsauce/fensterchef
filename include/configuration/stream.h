#ifndef CONFIGURATION__STREAM_H
#define CONFIGURATION__STREAM_H

/**
 * The stream abstracts over getting input.  The input may come from a string
 * or a file.  It gives consistent line endings (\n) and joins lines that have
 * special constructs with a new line followed by any amount of blacks and then
 * a backslash \.  It also skips comments which are lines starting with '#'.
 */

#include "utility/utility.h" /* utf8_t */

/* The stream struct.
 *
 * Only one is needed so this is simplified as a global object but easily
 * extendable to other object types.
 */
extern struct input_stream {
    /* the path of the file, this is `NULL` if the source is a string */
    const utf8_t *file_path;
    /* this is used to read up entire files (if it fits) */
    char local_string_storage[4096];
    /* input string */
    char *string;
    /* the current index within the string */
    size_t index;
    /* the length of the string */
    size_t length;
    /* how much data was allocated */
    size_t capacity;
} input_stream;

/* Initialize the internal stream object to parse file at given path.
 *
 * @path is not copied and must be valid as long as the stream is being used.
 *
 * @return ERROR if the file could not be opened, OK otherwise.
 */
int initialize_file_stream(const utf8_t *path);

/* Initialize the internal stream object to parse a given string.
 *
 * @return OK.
 */
int initialize_string_stream(const utf8_t *string);

/* Skip to the end of the line. */
void skip_stream_line(void);

/* Skip over any blank (`isblank()`). */
void skip_stream_blanks(void);

/* Skip over any white space (`isspace()`). */
void skip_stream_space(void);

/* Get the next character from given stream.
 *
 * @return EOF if the end has been reached.
 */
int get_stream_character(void);

/* Get the next character from given stream without advancing to the following
 * character.
 *
 * @return EOF if the end has been reached.
 */
int peek_stream_character(void);

/* Get the column and line of @index within the active stream.
 *
 * If @index is out of bounds, @line and @column are set to the last position in
 * the stream.
 */
void get_stream_position(size_t index, unsigned *line, unsigned *column);

/* Get the line within the current stream.
 *
 * @length will hold the length of the line.
 */
char *get_stream_line(unsigned line, unsigned *length);

#endif
