/*
 * permCheck.c - Check permission
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "lib.h"

void isRoot() {
	if (getuid() != 0) {
		fprintf(stderr, "%s: Need root (Try pass '--help')\n", getProgramName());
		exit(1);
	}
}
