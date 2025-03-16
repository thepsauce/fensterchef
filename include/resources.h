#ifndef RESOURCES_H
#define RESOURCES_H

#include "cursor.h"

/**
 * Management the X resources set on `RESOURCE_MANAGER`.
 */

/* the cached resources we are interested in */
extern struct resources {
    /* dots per inches of the screen */
    uint32_t dpi;
} resources;

/* Set a resource.
 *
 * The parameters make up this in X resources syntax:
 * If @is_loose is true:
 * <class>*<name>: <value>
 * and if @is_loose is false:
 * <class>.<name>: <value>
 *
 * <class> may be separated in:
 * <application>.<class>
 */
void set_resource(const char *class, const char *name, const char *value,
        bool is_loose);

/* Load the X resources from the root window's `RESOURCE_MANAGER` atom. */
void reload_resources(void);

#endif
