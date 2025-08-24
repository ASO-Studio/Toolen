/**
 *	basename.c - Return non-directory portion of a
 * 		     pathname removing suffix.
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

static void basename_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: basename PATH...\n\n"
			"Return non-directory portion of a pathname removing suffix.\n");
}

int basename_main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "basename: Need 1 argument\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		basename_show_help();
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		if (argv[i] && argv[i][0] != '-') {
			printf("%s\n", basename(argv[i]));
		}
	}

	return 0;
}

REGISTER_MODULE(basename);
