#ifndef LOG_H
#define LOG_H

#include "fensterchef.h"

#ifdef DEBUG

extern FILE *g_log_file;

#define LOG(fmt, ...) fprintf(g_log_file, (fmt), ##__VA_ARGS__); fflush(g_log_file)

/* Log an event to a file.  */
void log_event(xcb_generic_event_t *event, FILE *fp);

/* Log the screens information to a file. */
void log_screens(FILE *fp);

#else

#define LOG(fp, fmt, ...)

#endif // DEBUG

#endif
