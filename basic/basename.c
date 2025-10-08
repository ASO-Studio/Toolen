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

#if __has_include(<libgen.h>)
# include <libgen.h>
#else
# define NO_LIBGEN 1
#endif

#include "config.h"
#include "module.h"
#include "lib.h"

static void basename_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: basename PATH...\n\n"
			"Return non-directory portion of a pathname removing suffix.\n");
}

M_ENTRY(basename) {
	if (argc < 2) {
		pplog(P_HELP | P_NAME, "Need 1 argument");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		basename_show_help();
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		printf("%s\n",
#ifndef NO_LIBGEN
				basename(argv[i])
#else // !NO_LIBGEN
				lib_basename(argv[i]);
#endif // NO_LIBGEN
				);
	}

	return 0;
}

REGISTER_MODULE(basename);
