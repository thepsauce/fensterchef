#ifndef PARSE__PARSE_H
#define PARSE__PARSE_H

#include "parse/stream.h"

/* maximum value for a literal integer */
#define PARSE_INTEGER_LIMIT 1000000

/* the assumed width of a tab character \t */
#define PARSE_TAB_SIZE 8

/* the maximum number of files to deeply source */
#define PARSE_MAX_FILE_DEPTH 32

/* The maximum number of errors that can occur before the parser stops.
 * Outputting many errors is good as it helps with fixing files.  But when the
 * user sources an invalid file, the user should not be be flooded with errors.
 * We stop prematurely because of that.
 */
#define PARSE_MAX_ERROR_COUNT 30

/* Parse the given stream.
 *
 * The stream is activated using `initialize_file_stream()` or
 * `initialize_string_stream()`.
 *
 * This function prints the kind of error to `stderr`.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream(InputStream *stream);

/* Parse the given stream and use it to override the configuration.
 *
 * All parsed actions, bindings etc. are put into the configuration if this
 * function succeeds.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream_and_replace_configuration(InputStream *stream);

/* Parse the given stream and rull all actions within it.
 *
 * @return ERROR if a parsing error occured, OK otherwise.
 */
int parse_stream_and_run_actions(InputStream *stream);

#endif
