#ifndef CURSOR_H
#define CURSOR_H

#include "x11_management.h"

/* Load the cursor with given name using the user's preferred style. */
Cursor load_cursor(const char *name);

/* Clear all cursors within the cursor cache. */
void clear_cursor_cache(void);

#endif
