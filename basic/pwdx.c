/**
 *	pwdx.c - Print working directory of processes list on command line
 *
 * 	Created by RoofAlan
 *		2025/8/30
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void pwdx_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: pwdx PID...\n\n"
			"Print working directory of processes list on command line\n");
}

int pwdx_main(int argc, char *argv[]) {
	if (argc < 2) {
		pplog(P_HELP | P_NAME, "Need 1 argument");
		return 1;
	}

	// Parse argument
	if (findArg(argv, argc, "--help")) {
		pwdx_show_help();
		return 0;
	}

	int retValue = 0;

	// Main loop
	for (int i = 1; i < argc; i++) {
		pid_t pid = atoi(argv[i]);
		if (pid <= 0) {
			fprintf(stderr, "%s: Invalid PID\n", argv[i]);
			retValue = 1;
			continue;
		}
		char buf[1024];	// Output
		char path[1024];	// Input
		sprintf(path, "/proc/%d/cwd", pid);
		int len = 0;

		if((len = readlink(path, buf, sizeof(buf))) < 0) {
			pplog(P_ERRNO, "%d", pid);
			retValue = 1;
		} else {
			path[len - 1] = '\0';
			printf("%d: %s\n", pid, buf);
		}
	}

	return retValue;
}

REGISTER_MODULE(pwdx);
