#ifndef LOG_H
#define LOG_H

#include "fensterchef.h"

#ifdef DEBUG

#include <time.h>

extern FILE *g_log_file;

#define LOG(fmt, ...) do { \
    char buf[64]; \
    time_t cur_time; \
    struct tm *tm; \
    cur_time = time(NULL); \
    tm = localtime(&cur_time); \
    strftime(buf, sizeof(buf), "[%F %T] ", tm); \
    fputs(buf, g_log_file); \
    fprintf(g_log_file, (fmt), ##__VA_ARGS__); \
    fflush(g_log_file); \
} while (0)

#define ERR(fmt, ...) do { \
    char buf[64]; \
    time_t cur_time; \
    struct tm *tm; \
    cur_time = time(NULL); \
    tm = localtime(&cur_time); \
    strftime(buf, sizeof(buf), "{%F %T} ERR ", tm); \
    fputs(buf, g_log_file); \
    fprintf(g_log_file, (fmt), ##__VA_ARGS__); \
    fflush(g_log_file); \
} while (0)

/* Log an event to the log file.  */
void log_event(xcb_generic_event_t *event);

/* Log the screens information to the log file. */
void log_screens(void);

#else

#define LOG(fmt, ...)

#endif // DEBUG

#endif
