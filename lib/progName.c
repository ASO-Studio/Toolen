/*
 * progName.c - Get/Set current(working) program name
 */

#include <stdio.h>
#include "lib.h"

static char progname[1024];

char *getProgramName() {
	return progname;
}

void setProgramName(const char *n) {
	snprintf(progname, sizeof(progname), "%s", n);
}
