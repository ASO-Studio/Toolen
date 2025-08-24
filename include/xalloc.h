#ifndef _FALLOC_H
#define _FALLOC_H

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

#endif
