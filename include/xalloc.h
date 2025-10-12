/*
 * xalloc.h - Wrap of *alloc functions (header files)
 */

#ifndef _XALLOC_H
#define _XALLOC_H

#include <stddef.h>

// Encapsulated malloc
void *xmalloc(size_t size);

// Encapsulated calloc
void *xcalloc(size_t num, size_t size);

// Encapsulated realloc
void *xrealloc(void *ptr, size_t size);

// Encapsulated free
void xfree(void *ptr);

// Encapsulated strdup
char *xstrdup(const char *s);

// Disable exit
void xallocDisableExit();

// Enable exit
void xallocEnableExit();

#endif // _XALLOC_H
