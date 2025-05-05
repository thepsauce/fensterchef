#include <string.h>

#include <X11/Xcursor/Xcursor.h>

#include "cursor.h"
#include "log.h"
#include "utility/utility.h"
#include "x11_management.h"

/* The maximum of cached cursor entries.  It is actually lower than this because
 * the cache is only filled to a certain percentage.
 */
#define MAX_CURSOR_ENTRIES 64

/* The maximum inverse percentage the cache is allowed to be filled.
 *
 * Do not put this macro in brackets!
 */
#define CURSOR_CACHE_THRESHOLD 5/4

/* a cursor cache entry */
struct cursor_cache_entry {
    /* name of the cursor */
    char *name;
    /* the cursor this name resolves to */
    Cursor cursor;
};

/* the cursor cache */
static struct cursor_cache {
    /* the entries cached */
    struct cursor_cache_entry entries[MAX_CURSOR_ENTRIES];
    /* the number of filled entries */
    unsigned number_of_entries;
} Cache;

/* Load the cursor with given name using the user's preferred style. */
Cursor load_cursor(const char *name)
{
    uint32_t step = 0, index;
    Cursor cursor = None;

    /* use quadratic probing to find an index within the hash map */
    const uint32_t hash = get_fnv1_hash(name);
    do {
        index = hash + (step * step + step) / 2;
        index %= MAX_CURSOR_ENTRIES;
    } while (Cache.entries[index].name != NULL &&
            strcmp(Cache.entries[index].name, name) != 0);

    /* if the cursor is not cached yet */
    if (Cache.entries[index].name == NULL) {
        cursor = XcursorLibraryLoadCursor(display, name);
        if (cursor == None) {
            LOG_ERROR("could not load cursor: %s\n", name);
        /* add the cursor to the cache if it is not filled too much */
        } else if (Cache.number_of_entries * CURSOR_CACHE_THRESHOLD <
                MAX_CURSOR_ENTRIES) {
            LOG("cursor %s added to cache\n", name);
            Cache.entries[index].name = xstrdup(name);
            Cache.entries[index].cursor = cursor;
            Cache.number_of_entries++;
        }
    } else {
        cursor = Cache.entries[index].cursor;
    }

    return cursor;
}

/* Clear all cursors within the cursor cache. */
void clear_cursor_cache(void)
{
    for (unsigned i = 0;
            Cache.number_of_entries > 0 && i < MAX_CURSOR_ENTRIES;
            i++) {
        if (Cache.entries[i].name != NULL) {
            free(Cache.entries[i].name);
            XFreeCursor(display, Cache.entries[i].cursor);
            Cache.entries[i].name = NULL;
            Cache.number_of_entries--;
        }
    }
}
