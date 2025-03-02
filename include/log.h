#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#include "utility.h"

typedef enum {
    /* everything gets logged */
    SEVERITY_VERBOSE,
    /* only information gets logged */
    SEVERITY_INFO,
    /* only errors get logged */
    SEVERITY_ERROR,
    /* log nothing */
    SEVERITY_IGNORE,
} log_severity_t;

/* the severity of the logging */
extern log_severity_t log_severity;

#ifndef NO_ANSII_COLORS

/* ansi colors for colored output */
#define ANSI_BLACK 0
#define ANSI_RED 1
#define ANSI_GREEN 2
#define ANSI_YELLOW 3
#define ANSI_BLUE 4
#define ANSI_MAGENTA 5
#define ANSI_CYAN 6
#define ANSI_WHITE 7

/* clear the current visual attributes */
#define CLEAR_COLOR "\x1b[0m"
/* set the foreground color */
#define COLOR(color) "\x1b[3" STRINGIFY(ANSI_##color) "m"

#else

#define CLEAR_COLOR ""
#define COLOR(color) ""

#endif

/* wrappers around `log_formatted` for different severities */
#define LOG_VERBOSE(...) \
    log_formatted(SEVERITY_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG(...) \
    log_formatted(SEVERITY_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) \
    log_formatted(SEVERITY_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/* Print a formatted string to standard error output.
 *
 * The following format specifiers are supported on top of the reqular
 * printf-format specifiers (some printf-format specifiers might be
 * overwritten):
 * %P   int32_t, int32_t        X+Y
 * %S   uint32_t, uint32_t      WIDTHxHEIGHT
 * %R   uint32_t[4]             X+Y+WIDTHxHEIGHT
 * %w   xcb_window_t            ID<NUMBER or NAME>
 * %W   Window*                 ID<NUMBER or NAME>
 * %m   window_state_t          WINDOW_STATE
 * %F   Frame*                  (X+Y+WIDTHxHEIGHT)
 * %A   uint32_t, Action*       List of actions
 * %X   int                     CONNECTION ERROR
 * %a   xcb_atom_t              ATOM
 * %V   xcb_generic_event_t*    EVENT
 * %E   xcb_generic_error_t*    ERROR
 * %C   xcb_screen_t*           SCREEN
 */
void log_formatted(log_severity_t severity, const char *file, int line,
        const char *format, ...);

#endif
