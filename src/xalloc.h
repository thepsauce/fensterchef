#ifndef XALLOC_H
#define XALLOC_H

#include <stdlib.h>

/**
 * Like `malloc()` but exit when the allocation fails.
 */
void *xmalloc(size_t size);

/**
 * Like `malloc()` but exit when the allocation fails.
 */
void *xcalloc(size_t nmemb, size_t size);

/**
 * Like `realloc()` but exit when the allocation fails.
 */
void *xrealloc(void *ptr, size_t size);

/**
 * Like `reallocarray()` but exit when the allocation fails.
 */
void *xreallocarray(void *ptr, size_t nmemb, size_t size);

/**
 * Combination of `xmalloc()` and `memcpy()`.
 */
void *xmemdup(const void *ptr, size_t size);

/**
 * Like `strdup()` but exit when the allocation fails.
 */
void *xstrdup(const char *s);

/**
 * Like `strndup()` but exit when the allocation fails.
 */
void *xstrndup(const char *s, size_t n);

/**
 * Like `asprintf()` but exit on failure.
 */
char *xasprintf(const char *fmt, ...);

#endif

