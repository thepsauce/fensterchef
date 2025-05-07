#ifndef CONFIGURATION__PARSE_H
#define CONFIGURATION__PARSE_H

/* maximum value for a literal integer */
#define PARSE_INTEGER_LIMIT 1000000

/* the assumed width of a tab character \t */
#define PARSE_TAB_SIZE 8

/* the maximum number of files to deeply source */
#define PARSE_MAX_FILE_DEPTH 32

/* The maximum number of errors that can occur before the parser stops.
 * Outputting many errors is good as it helps with fixing files but when the
 * user sources an invalid file, the user should be be flooded with errors, we
 * stop prematurely because of that.
 */
#define PARSE_MAX_ERROR_COUNT 30

/* expands to all possible parse errors */
#define DEFINE_ALL_PARSE_ERRORS \
    /* indicates a successful parsing */ \
    X(PARSE_SUCCESS, "success") \
    /* could not open a file */ \
    X(PARSE_ERROR_INVALID_PATH, "invalid file path") \
    /* the token parser failed */ \
    X(PARSE_ERROR_INVALID_TOKEN, "invalid token") \

/* parse error codes */
typedef enum parse_error {
#define X(error, string) \
    error,
    DEFINE_ALL_PARSE_ERRORS
#undef X
} parse_error_t;

/* Parse the currently active stream.
 *
 * The stream is activated using `initialize_file_stream()` or
 * `initialize_string_stream()`.
 *
 * This function prints the kind of error to `stderr`.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream(void);

/* Parse the currently active stream and use it to override the configuration.
 *
 * All parsed actions, bindings etc. are put into the configuration if this
 * function succeeds.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream_and_replace_configuration(void);

/* Parse the currently active stream and rull all actions within it.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream_and_run_actions(void);

#endif
