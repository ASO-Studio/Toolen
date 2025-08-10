#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static int sync_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: sync\n\n"
			"Write pending cached data to disk, blocking until done\n");
}

int sync_main(int argc, char *argv[]) {
	// Parse arguments
	if (findArg(argv, argc, "--help")) {
		sync_show_help();
		return 0;
	} else if (findArg(argv, argc, "--version")) {
		JUST_VERSION();
		return 0;
	}

	sync();
	return 0;
}

REGISTER_MODULE(sync);
