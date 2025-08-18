#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "config.h"
#include "module.h"

static void pwd_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: pwd\n\n"
			"Print working(current) directory\n\n"
			"Support options:\n"
			"  -L   Print from environment($PWD)\n");
}

int pwd_main(int argc, char *argv[]) {
	int opt;
	int opt_index = 0;
	struct option opts[] = {
		{"L", no_argument, 0, 'L'},
		{"help", no_argument, 0, 'h'},
	};

	int useEnv = 0;

	// Parse arguments
	while((opt = getopt_long(argc, argv, "Lh", opts, &opt_index)) != -1) {
		switch(opt) {
			case 'L':
				useEnv = 1;
				break;
			case 'h':
				pwd_show_help();
				return 0;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
		}
	}

	// Print working directory from environment
	if (useEnv) {
		printf("%s\n", getenv("PWD") ? getenv("PWD") : "");
		return 0;
	}

	// Print from 'getcwd()'
	char workDir[512];
	getcwd(workDir, sizeof(workDir));
	printf("%s\n", workDir);
	return 0;
}

REGISTER_MODULE(pwd);
