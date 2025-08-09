#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void chroot_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: chroot NEWROOT [COMMAND] [ARGS]...\n\n"
			"Run command in NEWROOT (default run /bin/sh)\n");
}

int chroot_main(int argc, char *argv[]) {
	if(argc < 2) {
		fprintf(stderr, "chroot: requires 1 argument\n"
				"Try pass '--help' for more details\n");
		return 0;
	}

	// Parse arguments
	if(findArg(argv, argc, "--help")) {
		chroot_show_help();
		return 0;
	} else if(findArg(argv, argc, "--version")) {
		JUST_VERSION();
		return 0;
	}

	// We should change directory to NEWROOT first
	int status = chdir(argv[1]);
	status = chroot(".");	// Already in new root

	if (status < 0) {
		perror("chroot");
		return 1;
	}

	// Execute command in NEWROOT
	if (!argv[2]) {	// No command given
		char *args[] = {"/bin/sh", NULL};
		execvp("/bin/sh", args);
		perror("chroot");
		return 1;
	} else {
		// Execute user-given command
		execvp(argv[2], &argv[2]);
		perror("chroot");
		return 1;
	}

	return 0;
}

REGISTER_MODULE(chroot);
