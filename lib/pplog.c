/*
 * pplog.c - Program error output interface
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "lib.h"

int pplog(int flags, const char *msg, ...) {

	if (flags & P_NAME) {
		fprintf(stderr, "%s: ", getProgramName());
	}

	va_list args;
	va_start(args, msg);
	int ret = vfprintf(stderr, msg, args);
	va_end(args);

	if (flags & P_ERRNO) {
		fprintf(stderr, ": %s", strerror(errno));
	}
	if (flags & P_HELP) {
		fprintf(stderr, "\nTry pass '--help' for more details");
	}

	fprintf(stderr, "\n");

	return ret;
}
