/*
 * xio.h - Wrap of I/O functions (header files)
 */

#ifndef _XIO_H
#define _XIO_H

#include <stdio.h>
#include <sys/types.h>

// Encapsulated fopen
FILE *xfopen(const char *filename, const char *mode);

// Encapsulated fclose
void xfclose(FILE* fp);

// Encapsulated open
int xopen(const char *pathname, int flags, mode_t mode);

// Encapsulated open without argument 'mode'
int xopen2(const char *pathname, int flags);

// Encapsulated close
void xclose(int fd);

#endif // _XIO_H
