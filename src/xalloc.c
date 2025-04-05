#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utility.h"
#include "xalloc.h"

/* Allocate a minimum of @size bytes of memory. */
void *xmalloc(size_t size)
{
    void *pointer;

    if (UNLIKELY(size == 0)) {
        return NULL;
    }

    pointer = malloc(size);
    ASSERT(pointer != NULL, strerror(errno));
    return pointer;
}

/* Allocate @number_of_elements number of elements with each element being
 * @size_per_element large.
 */
void *xcalloc(size_t number_of_elements, size_t size_per_element)
{
    void *pointer;

    if (UNLIKELY(number_of_elements == 0 || size_per_element == 0)) {
        return NULL;
    }

    pointer = calloc(number_of_elements, size_per_element);
    ASSERT(pointer != NULL, strerror(errno));
    return pointer;
}

/* Grow or shrink a previously allocated memory region. */
void *xrealloc(void *pointer, size_t size)
{
    if (size == 0) {
        free(pointer);
        return NULL;
    }

    pointer = realloc(pointer, size);
    ASSERT(pointer != NULL, strerror(errno));
    return pointer;
}

/* Same as `xrealloc()` but instead of using bytes as argument, use
 * @number_of_elements * @size_per_element.
 */
void *xreallocarray(void *pointer, size_t number_of_elements, size_t size)
{
    size_t byte_count;

    ASSERT(!OVERFLOW_MULTIPLY(number_of_elements, size, byte_count),
            "unsigned integer overflow");
    return xrealloc(pointer, byte_count);
}

/* Combination of `xmalloc()` and `memcpy()`. */
void *xmemdup(const void *pointer, size_t size)
{
    char *duplicate;

    if (size == 0) {
        return NULL;
    }

    duplicate = malloc(size);
    ASSERT(duplicate != NULL, strerror(errno));

    return memcpy(duplicate, pointer, size);
}

/* Duplicates the null-terminated @string pointer by creating a copy. */
char *xstrdup(const char *string)
{
    size_t length;
    char *result;

    if (string == NULL) {
        return NULL;
    }

    /* +1 for the null terminator */
    length = strlen(string) + 1;
    result = xmalloc(length);
    return memcpy(result, string, length);
}

/* Like `xstrdup()` but stop at @length when the null-terminator is not yet
 * encountered.
 */
char *xstrndup(const char *string, size_t length)
{
    char *result;

    length = strnlen(string, length);
    /* +1 for the null terminator */
    result = xmalloc(length + 1);
    result[length] = '\0';
    return memcpy(result, string, length);
}

/* Combination of `xmalloc()` and `sprintf()`. */
char *xasprintf(const char *format, ...)
{
    va_list list;
    int total_size;
    char *result;

    /* get the length of the expanded format */
    va_start(list, format);
    total_size = vsnprintf(NULL, 0, format, list);
    va_end(list);

    ASSERT(total_size >= 0, strerror(errno));

    va_start(list, format);
    /* +1 for the null terminator */
    result = xmalloc(total_size + 1);
    (void) vsprintf(result, format, list);
    va_end(list);

    return result;
}
