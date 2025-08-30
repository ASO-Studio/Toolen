/**
 *	link.c - Create a hardlink to file
 *
 * 	Created by RoofAlan
 *		2025/8/30
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void link_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: link FILE NEWFILE\n\n"
			"Create a hardlink to file\n");
}

int link_main(int argc, char *argv[]) {
	if (argc < 3) {
		pplog(P_NAME | P_HELP, "Need 2 arguments");
		return 1;
	} else if (argc > 3) {
		pplog(P_NAME | P_HELP, "Max 2 arguments");
		return 1;
	}

	if (argc > 1 && findArg(argv, argc, "--help")) {
		link_show_help();
		return 0;
	}

	if (link(argv[1], argv[2]) < 0) {
		perror("link");
		return 1;
	}
	return 0;
}

REGISTER_MODULE(link);
