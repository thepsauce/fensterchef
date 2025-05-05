#include <string.h>
#include <wchar.h>

#include "configuration/parse.h"
#include "configuration/stream.h"

/* The stream struct.  */
struct input_stream input_stream = {
    .string = input_stream.local_string_storage,
    .capacity = sizeof(input_stream.local_string_storage)
};

/* Get a character from the stream without advancing the cursor. */
static inline int peek_character(void)
{
    if (input_stream.index == input_stream.length) {
        return EOF;
    }
    return (unsigned char) input_stream.string[input_stream.index];
}

/* Get the next character from stream. */
static INLINE int get_or_peek_stream_character(bool should_advance)
{
    int character, other;

    /* join lines when the next one starts with a backslash */
    while (true) {
        character = peek_character();
        if (character == EOF) {
            return EOF;
        }

        /* skip comment when there was a line end before */
        if (character == '#' &&
                (input_stream.index == 0 ||
                    islineend(input_stream.string[input_stream.index - 1]))) {
            /* skip over the end of the line */
            while (character = peek_character(),
                    !islineend(character) && character != EOF) {
                input_stream.index++;
            }

            if (character == EOF) {
                return EOF;
            }
            input_stream.index++;
        } else {
            if (should_advance) {
                input_stream.index++;
                if (!islineend(character)) {
                    return character;
                }
            } else {
                if (!islineend(character)) {
                    return character;
                }
                input_stream.index++;
            }
        }

        other = peek_character();
        /* put \r\n and \n\r together */
        if ((other == '\n' && character == '\r') ||
                (other == '\r' && character == '\n')) {
            input_stream.index++;
        }

        character = peek_character();
        /* let the above part take care of this */
        if (character == '#') {
            continue;
        }

        const size_t save_index = input_stream.index - 1;

        /* skip over any leading blanks */
        while (isblank(character)) {
            input_stream.index++;
            character = peek_character();
        }

        if (character != '\\') {
            if (!should_advance) {
                input_stream.index = save_index;
            }
            return '\n';
        }

        /* skip over \ */
        input_stream.index++;
    }
}

/* Get the next character from stream. */
int get_stream_character(void)
{
    return get_or_peek_stream_character(true);
}

/* Get the next character from given stream without advancing to the following
 * character.
 */
int peek_stream_character(void)
{
    return get_or_peek_stream_character(false);
}

/* Get the column and line of @index within the active stream. */
void get_stream_position(size_t index, unsigned *line, unsigned *column)
{
    unsigned current_line = 0, current_column = 0;
    int character;
    wchar_t wide_character;

    index = MIN(index, input_stream.length);
    for (size_t i = 0; i < index; i++) {
        character = (unsigned char) input_stream.string[i];
        if (isprint(character)) {
            current_column++;
        } else if (islineend(character)) {
            current_column = 0;
            current_line++;
            if (i + 1 < index) {
                character = (unsigned char) input_stream.string[i + 1];
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
                    &input_stream.string[i], input_stream.length - i);
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
char *get_stream_line(unsigned line, unsigned *length)
{
    size_t line_index = 0, end_line_index;
    size_t index = 0;
    int character;

    while (index < input_stream.length) {
        end_line_index = index;

        character = (unsigned char) input_stream.string[index];
        index++;
        if (islineend(character)) {
            if (index + 1 < input_stream.length) {
                character = (unsigned char) input_stream.string[index];
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
    return &input_stream.string[line_index];
}

/* Grow the memory the stream contains. */
static inline void grow_stream(void)
{
    if (input_stream.length > input_stream.capacity) {
        if (input_stream.string == input_stream.local_string_storage) {
            input_stream.string = NULL;
        }
        REALLOCATE(input_stream.string, input_stream.length);
        input_stream.capacity = input_stream.length;
    }
}

/* Initialize a stream object to parse file at given path. */
int initialize_file_stream(const utf8_t *path)
{
    FILE *file;
    long size;

    file = fopen(path, "r");
    if (file == NULL) {
        return ERROR;
    }

    input_stream.file_path = path;

    fseek(file, 0, SEEK_END);
    size = ftell(file);

    input_stream.index = 0;
    input_stream.length = size;
    grow_stream();

    fseek(file, 0, SEEK_SET);
    fread(input_stream.string, 1, input_stream.length, file);

    fclose(file);
    return OK;
}

/* Initialize a stream object to parse a given string. */
int initialize_string_stream(const utf8_t *string)
{
    input_stream.file_path = NULL;
    input_stream.index = 0;
    input_stream.length = strlen(string);
    grow_stream();
    memcpy(input_stream.string, string, input_stream.length);
    return OK;
}
