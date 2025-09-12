/**
 *	sync.c - Write pending cached data to disk, blocking
 *		 until done
 *
 * 	Created by RoofAlan
 *		2025/8/18
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void sync_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: sync\n\n"
			"Write pending cached data to disk, blocking until done\n");
}

M_ENTRY(sync) {
	// Parse arguments
	if (findArg(argv, argc, "--help")) {
		sync_show_help();
		return 0;
	}

	sync();
	return 0;
}

REGISTER_MODULE(sync);
