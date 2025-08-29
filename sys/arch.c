/**
 *	arch.c - Print machine(hardware) name, same as uname -m
 *
 * 	Created by RoofAlan
 *		2025/8/17
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>
#include <getopt.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void arch_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: arch\n\n"
			"Print machine(hardware) name, same as uname -m\n");
}

int arch_main(int argc, char *argv[]) {
	if (argc >= 2 && findArg(argv, argc, "--help")) {
		arch_show_help();
		return 0;
	}

	struct utsname uts;
	if(uname(&uts) < 0) {
		perror("uname");
		return 1;
	}

	printf("%s\n", uts.machine);
	return 0;
}

REGISTER_MODULE(arch);
