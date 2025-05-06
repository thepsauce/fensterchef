#include <ctype.h>
#include <string.h>

#include <X11/Xlib.h>

#include "configuration/parse.h"
#include "configuration/parse_struct.h"
#include "configuration/stream.h"

/* Skip to the beginning of the next line. */
void skip_line(void)
{
    int character;

    while (character = get_stream_character(),
            character != '\n' && character != EOF) {
        /* nothing */
    }
}

/* Skip over any blank (`isblank()`). */
void skip_blanks(void)
{
    while (isblank(peek_stream_character())) {
        (void) get_stream_character();
    }
}

/* Skip over any white space (`isspace()`). */
void skip_space(void)
{
    while (isspace(peek_stream_character())) {
        (void) get_stream_character();
    }
}

/* Skip to the next statement.
 *
 * This is used when an error occurs but more potential errors can be shown.
 */
void skip_statement(void)
{
    int character;

    while (character = get_stream_character(),
            character != ',' && character != '\n' && character != EOF) {
        /* skip over quotes */
        if (character == '\"' || character == '\'') {
            const int quote = character;
            while (parser.index = input_stream.index,
                    character = get_stream_character(),
                    character != quote && character != EOF &&
                    character != '\n') {
                /* nothing */
            }
        }
    }
}

/* Skip all following statements. */
void skip_all_statements(void)
{
    do {
        skip_statement();
        if (peek_stream_character() == ',') {
            (void) get_stream_character();
            continue;
        }
    } while (false);
}

/* Check if @character can be part of a word. */
static inline bool is_word_character(int character)
{
    if (iscntrl(character)) {
        return false;
    }

    if (character < 0) {
        return false;
    }

    if (character > 0x7f) {
        /* assume it is part of ascii */
        return true;
    }

    /* check for an alphanumeric and some additional characters */
    return isalnum(character) || character == '$' || character == '_' ||
        character == '-' ||
        /* # and % is needed for integers */
        character == '#' || character == '%';
}

/* Read a string/word from the active input stream. */
int read_string(void)
{
    int character;

    skip_space();

    parser.index = input_stream.index;
    parser.string_length = 0;
    character = peek_stream_character();
    if (character == '\"' || character == '\'') {
        parser.is_string_quoted = true;

        const int quote = character;

        /* skip over the opening quote */
        (void) get_stream_character();
        while (character = get_stream_character(),
                character != quote && character != EOF && character != '\n') {
            /* TODO: handle escape of quote */
            LIST_APPEND_VALUE(parser.string, character);
        }

        if (character != quote) {
            emit_parse_error("missing closing quote character");
        }
    } else {
        parser.is_string_quoted = false;

        /* read all until a new line or comma */
        while (character = peek_stream_character(),
                is_word_character(character)) {
            (void) get_stream_character();

            LIST_APPEND_VALUE(parser.string, character);
        }

        if (parser.string_length == 0) {
            return ERROR;
        }
    }

    /* append a nul-terminator but reduce the size again */
    LIST_APPEND_VALUE(parser.string, '\0');
    parser.string_length--;
    return OK;
}

/* Makes sure that next comes a word. */
void assert_read_string(void)
{
    if (read_string() != OK) {
        skip_all_statements();
        emit_parse_error("unexpected token");
        longjmp(parser.throw_jump, 1);
    }
}

/* Translate a string like "Button1" to a button index. */
int translate_string_to_button(const char *string)
{
    /* conversion from string to button index */
    static const struct button_string {
        /* the string representation of the button */
        const char *name;
        /* the button index */
        int button_index;
    } button_strings[] = {
        /* buttons can also be Button<integer> to directly address the index */
        { "LButton", 1 },
        { "LeftButton", 1 },

        { "MButton", 2 },
        { "MiddleButton", 2 },

        { "RButton", 3 },
        { "RightButton", 3 },

        { "ScrollUp", 4 },
        { "WheelUp", 4 },

        { "ScrollDown", 5 },
        { "WheelDown", 5 },

        { "ScrollLeft", 6 },
        { "WheelLeft", 6 },

        { "ScrollRight", 7 },
        { "WheelRight", 7 },

#define FIRST_X_BUTTON 8
#define NUMBER_OF_X_BUTTONS (UINT8_MAX - FIRST_X_BUTTON)
        /* X buttons (extra buttons on the mouse usually) going from X1 (8) to
         * X247 (255), they have their own handling and are not listed here
         */
    };

    int index = -1;

    /* parse strings starting with "X" */
    if (string[0] == 'X') {
        int x_index = 0;

        string++;
        while (isdigit(string[0])) {
            x_index *= 10;
            x_index += string[0] - '0';
            if (x_index > NUMBER_OF_X_BUTTONS) {
                return -1;
            }
            string++;
        }

        if (x_index == 0) {
            return -1;
        }

        index = FIRST_X_BUTTON + x_index - 1;
    /* parse strings starting with "Button" */
    } else if (strncmp(string, "Button", strlen("Button")) == 0) {
        index = 0;
        string += strlen("Button");
        while (isdigit(string[0])) {
            index *= 10;
            index += string[0] - '0';
            if (index > UINT8_MAX) {
                return -1;
            }
            string++;
        }
    } else {
        for (unsigned i = 0; i < SIZE(button_strings); i++) {
            if (strcmp(string, button_strings[i].name) == 0) {
                return button_strings[i].button_index;
            }
        }
    }

    if (string[0] != '\0') {
        return -1;
    }

    return index;
}

/* Try to resolve `parser.string` as integer.
 *
 * @return ERROR if `parser.string` is not an integer.
 */
int resolve_integer(void)
{
    /* a translation table from string to integer */
    static const struct {
        const char *string;
        parse_data_align_t integer;
    } string_to_integer[] = {
        { "true", 1 },
        { "false", 0 },
        { "on", 1 },
        { "off", 0 },
        { "yes", 1 },
        { "no", 0 },

        { "None", 0 },
        { "Shift", ShiftMask },
        { "Lock", LockMask },
        { "CapsLock", LockMask },
        { "Control", ControlMask },
        { "Alt", Mod1Mask },
        { "Mod1", Mod1Mask },
        { "Mod2", Mod2Mask },
        { "Mod3", Mod3Mask },
        { "Super", Mod4Mask },
        { "Mod4", Mod4Mask },
        { "Mod5", Mod5Mask },
    };

    char *word;
    int error = ERROR;
    parse_data_align_t integer = 0;

    word = parser.string;
    parser.data.flags = 0;
    if (isdigit(word[0])) {
        integer = word[0] - '0';
        word++;
        while (isdigit(word[0])) {
            integer *= 10;
            integer += word[0] - '0';
            word++;

            if (integer > PARSE_INTEGER_LIMIT) {
                emit_parse_error("integer overflows "
                        STRINGIFY(PARSE_INTEGER_LIMIT));
                do {
                    word++;
                } while (isdigit(word[0]));

                break;
            }
        }

        if (word[0] == 'p' && word[1] == 'x') {
            parser.data.flags |= PARSE_DATA_FLAGS_IS_PIXEL;
            word += 2;
        } else if (word[0] == '%') {
            parser.data.flags |= PARSE_DATA_FLAGS_IS_PERCENT;
            word++;
        }

        if (word[0] == '\0') {
            error = OK;
        }
    } else if (word[0] == '#') {
        word++;
        for (; isxdigit(word[0]); word++) {
            integer <<= 4;
            integer += isdigit(word[0]) ?
                    word[0] - '0' : tolower(word[0]) + 0xa - 'a';
        }
        if (integer > 0 && !(integer >> 24)) {
            integer |= 0xff << 24;
        }
        if (word[0] == '\0') {
            error = OK;
        }
    } else {
        for (unsigned i = 0; i < SIZE(string_to_integer); i++) {
            if (strcmp(string_to_integer[i].string, word) == 0) {
                integer = string_to_integer[i].integer;
                error = OK;
                break;
            }
        }
    }

    parser.data.u.integer = integer;
    return error;
}
