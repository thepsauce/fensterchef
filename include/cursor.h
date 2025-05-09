#ifndef CURSOR_H
#define CURSOR_H

#include <X11/X.h>

#include "utility/utility.h"

/* internal cache points for cursor constants */
typedef enum cursor {
    /* the cursor on the root window */
    CURSOR_ROOT,
    /* the cursor when moving a window */
    CURSOR_MOVING,
    /* the cursor when resizing horizontally */
    CURSOR_HORIZONTAL,
    /* the cursor when resizing vertically */
    CURSOR_VERTICAL,
    /* the cursor when sizing anything else */
    CURSOR_SIZING,

    CURSOR_MAX
} cursor_id_t;

/* Load the cursor with given name using the user's preferred style.
 *
 * @cursor_id is the point where the cursor is cached for repeated calls.
 * @name may be NULL to retrieve a cached cursor.  If none is cached, the
 *       default name is used to cache it.
 */
Cursor load_cursor(cursor_id_t cursor_id, _Nullable const char *name);

/* Clear all cursors within the cursor cache. */
void clear_cursor_cache(void);

#endif
