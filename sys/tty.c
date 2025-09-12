/**
 *	tty.c - Show filename of terminal connected to stdin
 *
 * 	Created by RoofAlan
 *		2025/8/26
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void tty_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: tty\n\n"
			"Show filename of terminal connected to stdin. If none print \"not a tty\" and exit with nonzero status.\n\n"
			"Support options:\n"
			"  -s Exit code only\n");
}

M_ENTRY(tty) {
	char *t = ttyname(0);
	int ret = !t;

	int show = 1;
	if (findArg(argv, argc, "-s")) {
		show = 0;
	}
	if (findArg(argv, argc, "--help") || findArg(argv, argc, "-h")) {
		tty_show_help();
		return 0;
	}

	if (show) {
		puts(t ? t : "not a tty");
	}

	return ret;
}

REGISTER_MODULE(tty);
