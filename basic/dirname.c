/**
 *	dirname.c - Show directory portion of path
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>

#if __has_include(<libgen.h>)
# include <libgen.h>
#else
# define NO_LIBGEN
#endif

#include "config.h"
#include "module.h"
#include "lib.h"

static void dirname_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: dirname PATH...\n\n"
			"Show directory portion of path\n");
}

M_ENTRY(dirname) {
	if (argc < 2) {
		pplog(P_NAME | P_HELP, "Need 1 argument");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		dirname_show_help();
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		printf("%s\n",
#ifndef NO_LIBGEN
				dirname(argv[i])
#else // !NO_LIBGEN
				lib_dirname(argv[i])
#endif // NO_LIBGEN
				);
	}

	return 0;
}

REGISTER_MODULE(dirname);
