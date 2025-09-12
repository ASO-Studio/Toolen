#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/swap.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "config.h"
#include "module.h"
#include "lib.h"

#ifndef SWAP_FLAG_DISCARD
# define SWAP_FLAG_DISCARD 0x10000
#endif

// Display help information in coreutils style
static void swapon_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: swapon [OPTION] filename\n\n"
			"Enable swapping on a given device/file.\n\n"
			"  -d      Discard freed SSD pages\n"
			"  -p      Priority (highest priority areas allocated first)\n");
}

M_ENTRY(swapon) {
	int opt;
	int discard_flag = 0;
	int priority = -1;  // Default: no priority specified
	char *endptr;
	
	// Long option definitions
	struct option long_options[] = {
		{"priority", required_argument, 0, 'p'},
		{"help",	 no_argument,	   0, 'h'},
		{0, 0, 0, 0}
	};

	// Parse command-line options
	while ((opt = getopt_long(argc, argv, "dp:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'h': // Handle long options
			swapon_show_help();
			return 0;
		case 'd':
			discard_flag = 1;
			break;
		case 'p':
			priority = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || priority < 0) {
				pplog(P_NAME, "Invalid priority value: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		default:
			fprintf(stderr, "Try 'swapon --help' for more information.\n");
			return EXIT_FAILURE;
		}
	}

	// Check permission
	isRoot();

	// Verify filename argument exists
	if (optind >= argc) {
		pplog(P_NAME | P_HELP, "Missing filename argument\n");
		return EXIT_FAILURE;
	}
	const char *filename = argv[optind];

	// Build flags for swapon()
	int flags = 0;
	if (priority != -1) {
		flags |= (priority & SWAP_FLAG_PRIO_MASK) | SWAP_FLAG_PREFER;
	}
	if (discard_flag) {
		flags |= SWAP_FLAG_DISCARD;
	}

	// Execute system call
	if (swapon(filename, flags) == -1) {
		fprintf(stderr, "swapon failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

REGISTER_MODULE(swapon);
