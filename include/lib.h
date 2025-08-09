#ifndef _LIB_H
#define _LIB_H

/* Find string in string group */
int findArg(char *args[], int argc, const char *str);

/* Check is it a directory */
int isDirectory(const char *path);

/* Convert time expression to seconds */
unsigned long timeToSeconds(const char *str);

#endif // _LIB_H
