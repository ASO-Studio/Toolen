/**
 *	swapoff.c - Disable swapping on a device or file
 *
 * 	Created by RoofAlan
 *		2025/8/11
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <sys/swap.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void swapoff_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: swapoff FILE\n\n"
			"Disable swapping on a device or file\n");
}

M_ENTRY(swapoff) {
	if (argc != 2) {
		pplog(P_NAME | P_HELP, "Required 2 arguments");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		swapoff_show_help();
		return 0;
	}

	isRoot(); // Need root

	if (swapoff(argv[1]) < 0) {
		pplog(P_NAME | P_ERRNO, "failed to disable swapping on '%s'", argv[1]);
		return 1;
	}
	return 0;
}

REGISTER_MODULE(swapoff);
