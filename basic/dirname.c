/**
 *	basename.c - Show directory portion of path
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <libgen.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void dirname_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: dirname PATH...\n\n"
			"Show directory portion of path\n");
}

int dirname_main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "dirname: Need 1 argument\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		dirname_show_help();
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		printf("%s\n", dirname(argv[i]));
	}

	return 0;
}

REGISTER_MODULE(dirname);
