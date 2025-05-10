#ifndef PARSE__UTILITY_H
#define PARSE__UTILITY_H

#include <X11/Xlib.h>

/**
 * Parse very simple constructs like repeated space and resolve integers and
 * strings.
 *
 * This file is meant to be private to the parser.
 */

/* Skip to the beginning of the next line. */
void skip_line(void);

/* Skip over any blank (`isblank()`). */
void skip_blanks(void);

/* Skip over any white space (`isspace()`). */
void skip_space(void);

/* Skip to the next statement.
 *
 * This is used when an error occurs but more potential errors can be shown.
 */
void skip_statement(void);

/* Skip all following statements. */
void skip_all_statements(void);

/* Read a string/word from the active input stream.
 *
 * @return ERROR if there was no string, OK otherwise.
 */
int read_string(void);

/* Makes sure that next comes a word.
 *
 * If no word comes next, an error is thrown.
 */
void assert_read_string(void);

/* Translate a string like "Button1" to a button index.
 *
 * @return `BUTTON_NONE` if the button string is invalid.
 */
int translate_string_to_button(const char *string);

/* Try to resolve `parser.word` as integer.
 *
 * The result is stored in `parser.integer`.
 *
 * @return ERROR if `parser.word` is not an integer.
 */
int resolve_integer(void);

/* Translate a string to some extended key symbols. */
KeySym translate_string_to_additional_key_symbols(const char *string);

#endif
