/**
 *	clear.c - Clear screen
 *
 * 	Created by RoofAlan
 *		2025/8/1
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>

#include "module.h"
#include "config.h"

static void clear_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: clear\n\n"
		"Clear screen\n");
}

int clear_main(int argc, char *argv[]) {
	if (argv[1]) {
		if(strcmp(argv[1], "--help") == 0) {
			clear_show_help();
			return 0;
		}
	}
	printf("\e[H\e[2J\e[3J");
	return 0;
}

REGISTER_MODULE(clear);
