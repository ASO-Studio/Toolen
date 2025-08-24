#ifndef _LIB_H
#define _LIB_H

#include "__getch.h"
#include "xalloc.h"

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
char **parse_command(const char *input, const char *delimiters);	// return: null->false, other->true

#endif // _LIB_H
