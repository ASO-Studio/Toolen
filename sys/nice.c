/**
 *	nice.c - Run a command line at an increased or decreased scheduling priority
 *
 * 	Created by RoofAlan
 *		2025/9/6
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include "config.h"
#include "module.h"
#include "lib.h"
#include "debug.h"

static void nice_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: nice [OPTIONS] PROGRAM [ARGS]...\n\n"
			"Run a command line at an increased or decreased scheduling priority.\n\n"
			"Support options:\n"
			"  -n NUM  Add given adjustment to priority (default 10)\n");
}

M_ENTRY(nice) {
	if (argc < 2) {
		pplog(P_NAME | P_HELP, "missing program");
		return 1;
	}

	int prio = 10;

	if (findArg(argv, argc, "--help")) {
		nice_show_help();
		return 0;
	}

	int i = 1;
	for (; i < argc - 1; i++) {
		if (strcmp(argv[i], "-n") == 0) {
			if (!argv[i+1]) {	// -n [NULL]
				pplog(P_NAME | P_HELP, "missing number");
				return 1;
			} else {
				prio = atoi(argv[++i]);
				i++;
				break;
			}
		}
	}

	if (setpriority(PRIO_PROCESS, 0, prio) < 0) {
		pplog(P_NAME | P_ERRNO, "setpriority() failed");
		return 1;
	}

	LOG("Program: %s\n", argv[i]);
	execvp(argv[i], &argv[i]);
	pplog(P_NAME | P_ERRNO, "cannot execute program");

	return 0;
}

REGISTER_MODULE(nice);
