#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "module.h"
#include "lib.h"

// Drop privileges and execute another program
static int execby(const char *username, const char *program, char *const argv[]) {
	uid_t uid = getuid_name(username);
	gid_t gid = getgid_name(username);

	// Set group
	if (setgroups(0, NULL) != 0) {
		pplog(P_ERRNO | P_NAME, "failed to set group");
		return -1;
	}

	// Set group ID
	if (setgid(gid) != 0) {
		pplog(P_ERRNO | P_NAME, "failed to set GID");
		return -1;
	}

	// Set user ID
	if (setuid(uid) != 0) {
		pplog(P_ERRNO | P_NAME, "failed to set UID");
		return -1;
	}

	// Verify
	if (getuid() != uid || geteuid() != uid) {
		fprintf(stderr, "Failed to set UID and GID completely\n");
		return -1;
	}

	// Execute the target program
	execvp(program, argv);

	// If we get here, execvp failed
	perror("Failed to execute program");
	return -1;
}

static void execby_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: execby USERNAME PROGRAM [ARGS]...\n\n"
			"Execute PROGRAM as user USERNAME\n");
}

int execby_main(int argc, char *argv[]) {
	if (argc < 2) {
		pplog(P_HELP, "Usage: execby USERNAME PROGRAM [ARGS]...");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		execby_show_help();
		return 0;
	} else if (argc < 3) {
		pplog(P_HELP | P_NAME, "Missing PROGRAM or USERNAME");
		return 1;
	}

	const char *username = argv[1];
	const char *program = argv[2];
	char *const *program_args = &argv[2]; // args start from argv[2]

	if (execby(username, program, program_args) != 0) {
		return 1;
	}

	return 0;
}

REGISTER_MODULE(execby);
