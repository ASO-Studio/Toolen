/**
 *	unlink.c - Delete one file
 *
 * 	Created by RoofAlan
 *		2025/8/23
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void unlink_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: unlink FILE\n\n"
			"Delete one file\n");
}

int unlink_main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "unlink: Need 1 argument\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	if (argc > 1 && findArg(argv, argc, "--help")) {
		unlink_show_help();
		return 0;
	}

	if (unlink(argv[1]) < 0) {
		perror("unlink");
		return 1;
	}
	return 0;
}

REGISTER_MODULE(unlink);
