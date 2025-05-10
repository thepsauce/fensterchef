#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "parse/parse.h"
#include "parse/stream.h"
#include "utility/utility.h"

/* Get a character from the stream without advancing the cursor. */
static inline int peek_character(InputStream *stream)
{
    if (stream->index == stream->length) {
        return EOF;
    }
    return (unsigned char) stream->string[stream->index];
}

/* Get the next character from stream. */
static INLINE int get_or_peek_stream_character(InputStream *stream,
        bool should_advance)
{
    int character, other;

    /* join lines when the next one starts with a backslash */
    while (true) {
        character = peek_character(stream);
        if (character == EOF) {
            return EOF;
        }

        /* skip comment when there was a line end before */
        if (character == '#' &&
                (stream->index == 0 ||
                    islineend(stream->string[stream->index - 1]))) {
            /* skip over the end of the line */
            while (character = peek_character(stream),
                    !islineend(character) && character != EOF) {
                stream->index++;
            }

            if (character == EOF) {
                return EOF;
            }
            stream->index++;
        } else {
            if (should_advance) {
                stream->index++;
                if (!islineend(character)) {
                    return character;
                }
            } else {
                if (!islineend(character)) {
                    return character;
                }
                stream->index++;
            }
        }

        other = peek_character(stream);
        /* put \r\n and \n\r together */
        if ((other == '\n' && character == '\r') ||
                (other == '\r' && character == '\n')) {
            stream->index++;
        }

        character = peek_character(stream);
        /* let the above part take care of this */
        if (character == '#') {
            continue;
        }

        const size_t save_index = stream->index - 1;

        /* skip over any leading blanks */
        while (isblank(character)) {
            stream->index++;
            character = peek_character(stream);
        }

        if (character != '\\') {
            if (!should_advance) {
                stream->index = save_index;
            }
            return '\n';
        }

        /* skip over \ */
        stream->index++;
    }
}

/* Get the next character from stream. */
int get_stream_character(InputStream *stream)
{
    return get_or_peek_stream_character(stream, true);
}

/* Get the next character from given stream without advancing to the following
 * character.
 */
int peek_stream_character(InputStream *stream)
{
    return get_or_peek_stream_character(stream, false);
}

/* Get the column and line of @index within the active stream. */
void get_stream_position(const InputStream *stream, size_t index,
        unsigned *line, unsigned *column)
{
    unsigned current_line = 0, current_column = 0;
    int character;
    wchar_t wide_character;

    index = MIN(index, stream->length);
    for (size_t i = 0; i < index; i++) {
        character = (unsigned char) stream->string[i];
        if (isprint(character)) {
            current_column++;
        } else if (islineend(character)) {
            current_column = 0;
            current_line++;
            if (i + 1 < index) {
                character = (unsigned char) stream->string[i + 1];
                if (islineend(character)) {
                    i++;
                }
            }
        } else if (character == '\t') {
            current_column = current_column -
                current_column % PARSE_TAB_SIZE;
        } else if (character < ' ') {
            /* these characters are printed as ? */
            current_column++;
        } else {
            const int result = mbtowc(&wide_character,
                    &stream->string[i], stream->length - i);
            if (result == -1) {
                /* these characters are printed as ? */
                current_column++;
            } else {
                index += result - 1;
                current_column += wcwidth(wide_character);
            }
        }
    }

    *line = current_line;
    *column = current_column;
}

/* Get the beginning of the line at given line index. */
const char *get_stream_line(const InputStream *stream, unsigned line,
        unsigned *length)
{
    size_t line_index = 0, end_line_index;
    size_t index = 0;
    int character;

    while (index < stream->length) {
        end_line_index = index;

        character = (unsigned char) stream->string[index];
        index++;
        if (islineend(character)) {
            if (index + 1 < stream->length) {
                character = (unsigned char) stream->string[index];
                if (islineend(character)) {
                    index++;
                }
            }
            if (line == 0) {
                index = end_line_index;
                break;
            }
            line_index = index;
            line--;
        }
    }

    *length = index - line_index;
    return &stream->string[line_index];
}

/* Initialize a stream object to parse file at given path. */
InputStream *create_file_stream(const utf8_t *path)
{
    FILE *file;
    long size;
    InputStream *stream;

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);

    stream = xmalloc(sizeof(*stream) + size);
    stream->file_path = xstrdup(path);
    stream->index = 0;
    stream->length = size;

    fseek(file, 0, SEEK_SET);
    fread(stream->string, 1, size, file);

    fclose(file);

    return stream;
}

/* Initialize a stream object to parse a given string. */
InputStream *create_string_stream(const utf8_t *string)
{
    size_t size;
    InputStream *stream;

    size = strlen(string);

    stream = xmalloc(sizeof(*stream) + size);
    stream->file_path = NULL;
    stream->index = 0;
    stream->length = size;

    COPY(stream->string, string, size);
    return stream;
}

/* Destroy a previously allocated input stream object. */
void destroy_stream(InputStream *stream)
{
    if (stream != NULL) {
        free(stream->file_path);
        free(stream);
    }
}
