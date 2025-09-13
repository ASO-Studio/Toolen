/**
 *	whoami.c - Print current user name
 *
 * 	Created by RoofAlan
 *		2025/8/19
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "module.h"
#include "config.h"
#include "lib.h"

static void whoami_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: %s\n\n"
			"Print current user name\n", getProgramName());
}

M_ENTRY(whoami) {
	if(findArg(argv, argc, "--help")) {
		whoami_show_help();
		return 0;
	}

	printf("%s\n", get_username(getuid()) ? : "unknown");
	return 0;
}

REDIRECT(whoami, logname);
REGISTER_MODULE(whoami);
