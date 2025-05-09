#include <string.h>

#include <X11/Xcursor/Xcursor.h>

#include "cursor.h"
#include "log.h"
#include "utility/utility.h"
#include "x11_synchronize.h"

/* the cursor cache */
static struct cursor_cache_entry {
    /* the default name for this cursor */
    const char *default_name;
    /* the X cursor */
    Cursor cursor;
} cursor_cache[CURSOR_MAX] = {
    { "left_ptr", None },
    { "fleur", None },
    { "sb_h_double_arrow", None },
    { "sb_v_double_arrow", None },
    { "sizing", None },
};

/* Load the cursor with given name using the user's preferred style. */
Cursor load_cursor(cursor_id_t cursor_id, const char *name)
{
    Cursor cursor = None;

    /* if the cursor is not cached yet */
    if (cursor_cache[cursor_id].cursor == None || name != NULL) {
        if (name == NULL) {
            name = cursor_cache[cursor_id].default_name;
        }

        cursor = XcursorLibraryLoadCursor(display, name);
        if (cursor == None) {
            LOG_ERROR("could not load cursor: %s\n",
                    name);

            /* try to use the default name if not already tried */
            if (cursor_cache[cursor_id].default_name != NULL) {
                cursor = load_cursor(cursor_id,
                        cursor_cache[cursor_id].default_name);
            }
        } else {
            LOG("cursor %s added to cache\n",
                    name);
            if (cursor_cache[cursor_id].cursor != None) {
                XFreeCursor(display, cursor_cache[cursor_id].cursor);
            }
            cursor_cache[cursor_id].cursor = cursor;
        }
    } else {
        cursor = cursor_cache[cursor_id].cursor;
    }

    return cursor;
}

/* Clear all cursors within the cursor cache. */
void clear_cursor_cache(void)
{
    for (cursor_id_t i = 0; i < CURSOR_MAX; i++) {
        if (cursor_cache[i].cursor != None) {
            XFreeCursor(display, cursor_cache[i].cursor);
            cursor_cache[i].cursor = None;
        }
    }
}
