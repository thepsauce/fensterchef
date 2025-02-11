#ifndef LOG_H
#define LOG_H

#ifdef DEBUG

#include <stdio.h>
#include <time.h>
#include <xcb/xcb_event.h>

#include "fensterchef.h"

/* Log a formatted message. */
#define LOG(format, ...) do { \
    char time_buffer[64]; \
    time_t current_time; \
    struct tm *tm; \
    current_time = time(NULL); \
    tm = localtime(&current_time); \
    strftime(time_buffer, sizeof(time_buffer), "[%F %T]", tm); \
    fputs(time_buffer, stderr); \
    fprintf(stderr, "(%s:%d) ", __FILE__, __LINE__); \
    fprintf(stderr, (format), ##__VA_ARGS__); \
} while (0)

#define LOG_ADDITIONAL(format, ...) do { \
    fprintf(stderr, (format), ##__VA_ARGS__); \
} while (0)

/* Log a formatted message with error indication. */
#define ERR(format, ...) do { \
    char time_buffer[64]; \
    time_t current_time; \
    struct tm *tm; \
    current_time = time(NULL); \
    tm = localtime(&current_time); \
    strftime(time_buffer, sizeof(time_buffer), "{%F %T}", tm); \
    fputs(time_buffer, stderr); \
    fprintf(stderr, "(%s:%d) ERR ", __FILE__, __LINE__); \
    fprintf(stderr, (format), ##__VA_ARGS__); \
} while (0)

/* Log an event to the log file.  */
void log_event(xcb_generic_event_t *event);

/* Log an xcb error to the log file with additional output formatting and new
 * line.
 */
void log_error(xcb_generic_error_t *error, const char *fmt, ...);

/* Log the screen information to the log file. */
void log_screen(void);

#else

#define LOG(...)
#define LOG_ADDITONAL(...)
#define ERR(...)

#define log_event(...)
#define log_error(...)
#define log_screen(...)

#endif // DEBUG

#endif
