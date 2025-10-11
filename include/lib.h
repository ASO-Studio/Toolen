/*
 * lib.h - Declarations of library functions
 */

#ifndef _LIB_H
#define _LIB_H

#include "__getch.h"
#include "xalloc.h"
#include "xio.h"
#include "defs.h"
#include "userInfo.h"
#include "pplog.h"
#include "elfop.h"

#define UNSIG(x) ((unsigned char*)x)

/* Find string in string group */
int findArg(char *args[], int argc, const char *str);	// return: 1->true, 0->false

/* Check is it a directory */
int isDirectory(const char *path);	// return: 1->true, 0->false

/* Convert time expression to seconds */
unsigned long timeToSeconds(const char *str);	// return: microsecond

/* Generate UUID (Version 4) */
void uuidGen(char *uuid);	// Non-return, output->@arg1

/* Execute command and redirect to pipe */
char *execInPipe(const char *command, char *const *args);	// return: command output

/* Check if a string is equation */
int isEquation(const char *str);	// return: 1->true, 0->false

/* Split command */
char **parse_command(const char *input, const char *delimiters, int remove_quotes);	// return: null->false, other->true

/* Get program name */
char *getProgramName();	// return: all->program name

/* Set program name */
void setProgramName(const char *n); // Non-return

/* Check if program did not run with root permission */
void isRoot();	// Non-return, NON-ROOT->EXIT(1)

/* basename() implementation */
char *lib_basename(char *path);

/* dirname() implementation */
char *lib_dirname(char *path);

/* Get terminal size */
int getTerminalSize(int *col, int *row);

/* Line edit */
char *xreadline(const char *pmpt);

#endif // _LIB_H
