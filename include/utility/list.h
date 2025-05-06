#ifndef UTILITY__LIST_H
#define UTILITY__LIST_H

/**
 * It became an increasing effort to maintain lists with count and capacity.
 * That is why this abstract list implementation was created.  It has the exact
 * same efficiency and semantics as using standard methods but helps out with a
 * bunch of macros.
 */

#include <string.h>

#include "utility/utility.h"

/* Define a list of items with capacity.
 *
 * Type @type is the type of the list.
 * T*   @name is the name of the list.
 *
 * void @return expand to all variables required for the list.
 */
#define LIST(type, name) \
    type *name; \
    size_t name##_length; \
    size_t name##_capacity

/* Grow the capacity of the list to at least the given capacity.
 *
 * T*     @name is the name of the list.
 * size_t @capacity is the minimum capacity.
 *
 * void @return
 */
#define LIST_GROW(name, capacity) do { \
    const size_t _capacity = (capacity); \
\
    if (_capacity >= name##_capacity) { \
        name##_capacity += _capacity; \
        REALLOCATE(name, name##_capacity); \
    } \
} while (0)

/* Set the items of a list.
 *
 * T*     @name is the name of the list.
 * size_t @from is the index to start setting from.  It must be at most the size
 *              of the list.
 * T*     @items is the items to set.
 * size_t @item_count is the number of items.
 *
 * void @return
 */
#define LIST_SET(name, from, items, item_count) do { \
    const size_t _from = (from); \
    const void *_items = (items); \
    const size_t _item_count = (item_count); \
\
    LIST_GROW(name, _from + _item_count); \
    name##_length = _from + _item_count; \
    if (_items == NULL) { \
        ZERO(&name[_from], item_count); \
    } else { \
        COPY(&name[_from], _items, item_count); \
    } \
} while (0)

/* Append an element to a list.
 *
 * T*     @name is the name of the list.
 * T*     @items is the items to append.
 * size_t @item_count is the number of items
 *
 * void @return
 */
#define LIST_APPEND(name, items, item_count) \
    LIST_SET(name, name##_length, items, item_count)

/* Append an element to a list.
 *
 * T* @name is the name of the list.
 * T  @value is the value to append.
 *
 * void @return
 */
#define LIST_APPEND_VALUE(name, value) do { \
    LIST_GROW(name, name##_length + 1); \
    name[name##_length] = (value); \
    name##_length++; \
} while (0)

/* Copy elements from a list.
 *
 * T*     @name is the name of the list.
 * size_t @from is the index of the first element to copy.
 * size_t @to is the first index of the element not to copy.
 * T*     @target is a pointer to the target memory region.
 *
 * void @return
 */
#define LIST_COPY(name, from, to, target) do { \
    const size_t _from = (from); \
    const size_t _to = (to); \
\
    ALLOCATE(target, _to - _from); \
    COPY(target, &name[_from], _to - _from); \
} while (0)

/* Copy all elements from a list to a target memory region.
 *
 * T* @name is the name of the list.
 * T* @target is a pointer to the target memory region.
 * 
 * void @return
 */
#define LIST_COPY_ALL(name, target) \
    LIST_COPY(name, 0, name##_length, target)

#endif
