/**
 *	yes.c - Print a string(or 'y') forever
 *
 * 	Created by RoofAlan
 *		2025/8/18
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>

#include "module.h"
#include "config.h"

static void yes_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr,
		"Usage: yes [String]\n"
		"   or: yes [Options]\n"
		"  --help  show this page\n");
}

M_ENTRY(yes) {
	(void)argc;
	const char *strToShow = "y";

	if (argv[1] && argv[1][0] == '-') {	// It's an option
		if(strcmp(argv[1], "--help") == 0) {
			yes_show_help();
			return 0;
		} else {
			fprintf(stderr, "Unknown option -- '%s'\n", argv[1]);
			fprintf(stderr, "Try pass '--help' for more details\n");
			return 1;
		}
	} else if (argv[1]) {
		strToShow = argv[1];
	}

	while(1) {
		puts(strToShow);
	}
	return 0;
}

REGISTER_MODULE(yes);
