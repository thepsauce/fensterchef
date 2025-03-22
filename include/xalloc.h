#ifndef XALLOC_H
#define XALLOC_H

/* Heap allocation functions.
 *
 * If at any point there is not enough heap space while using these functions,
 * the program is aborted.
 */

#include <stdlib.h>

/* Allocate a minimum of @size bytes of memory.
 *
 * The allocated memory is uninitialized.
 *
 * @return the start of the allocated region or NULL when @size is 0.
 */
void *xmalloc(size_t size);

/* Allocate @number_of_elements number of elements with each element being
 * @size_per_element large.
 *
 * This allocates a minimum of @number_of_elements * @size_per_element bytes.
 * If this product overflows, the program is aborted.
 *
 * The allocated memory is initialized to 0.
 *
 * @return the start of the allocated region or NULL when the byte count is 0.
 */
void *xcalloc(size_t number_of_elements, size_t size_per_element);

/* Grow or shrink a previously allocated memory region.
 *
 * @pointer is a pointer to the beginning of a previously allocated region.
 * @size is the number of bytes to shrink/grow the memory region to.
 *
 * @return the new pointer to the memory region. This might me the same as
 *         @pointer but this is often not the case when growing.
 *         This is NULL if @size is 0.
 */
void *xrealloc(void *pointer, size_t size);

/* Same as `xrealloc()` but instead of using bytes as argument, use
 * @number_of_elements * @size_per_element.
 *
 * If this product overflows, the program is aborted.
 */
void *xreallocarray(void *pointer, size_t number_of_elements,
        size_t size_per_element);

/* Combination of `xmalloc()` and `memcpy()`. */
void *xmemdup(const void *pointer, size_t size);

/* Duplicate the null-terminated @string pointer by creating a copy.
 *
 * @string may be NULL, then NULL is returned.
 */
char *xstrdup(const char *string);

/* Like `xstrdup()` but stop at @length when the null-terminator is not yet
 * encountered.
 */
char *xstrndup(const char *string, size_t length);

/* Combination of `xmalloc()` and `sprintf()`.
 *
 * This first figures out the needed amount of bytes for @format and given
 * variable arguments, then allocates memory to hold.
 */
char *xasprintf(const char *format, ...);

#endif

