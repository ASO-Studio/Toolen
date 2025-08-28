/**
 *	mkdir.c - Create directories
 *
 * 	Created by RoofAlan
 *		2025/8/28
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

bool verbose = false;	// For options -v

// create_parent - Create parent directories for a given path
static int create_parent(const char *path, mode_t mode) {
	char *path_copy = strdup(path);
	if (!path_copy) {
		perror("strdup");
		return -1;
	}
	
	char *p = path_copy;
	int result = 0;
	
	// Skip leading slash for absolute paths
	if (*p == '/') {
		p++;
	}
	
	while (*p != '\0') {
		// Find next path separator
		while (*p != '\0' && *p != '/') {
			p++;
		}
		
		// Temporarily terminate the string at this position
		char temp = *p;
		if (temp != '\0') {
			*p = '\0';
		}
		
		// Create directory if it doesn't exist
		if (mkdir(path_copy, mode) < 0) {
			if (errno != EEXIST) {  // Ignore if directory already exists
				perror("mkdir");
				result = -1;
				break;
			}
		}
		if (verbose) printf("mkdir: created directory '%s'\n", path_copy);
		
		// Restore the path separator and move to next component
		if (temp != '\0') {
			*p = temp;
			p++;
		}
	}
	
	free(path_copy);
	return result;
}

static void mkdir_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: mkdir [OPTIONS] DIR...\n\n"
			"Create one or more directories\n\n"
			"Support options:\n"
			"  -p	  Make parent directories(--parent)\n"
			"  -m MODE Set permission for directories(--mode)\n"
			"  -v	  Verbose output(--verbose)\n");
}

int mkdir_main(int argc, char *argv[]) {
	int opt;
	int option_index = 0;

	struct option ops[] = {
		{"verbose", no_argument, NULL, 'v'},
		{"mode", required_argument, NULL, 'm'},
		{"parent", no_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	bool makeParent = false;	// For option -p
	mode_t perm = 0777;		// For option -m (default: 0777)

	// If argc < 2, means no arguments gave
	if (argc < 2) {
		fprintf(stderr, "mkdir: need at least 1 argument!\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	// Parse arguments
	while ((opt = getopt_long(argc, argv, "vpm:", ops, &option_index)) != -1) {
		switch(opt) {
			case 'v':
				verbose = true;
				break;
			case 'p':
				makeParent = true;
				break;
			case 'm':
				if (optarg) {
					perm = atoi(optarg);
					if (perm > 4095) {
						fprintf(stderr, "mkdir: Invalid mode: %u\n"
								"Try pass '--help' for more details\n", perm);
						return 1;
					}
				}
				break;
			case 'h':
				mkdir_show_help();
				return 0;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
			default:
				abort();
		}
	}

	if (argc - optind <= 0) {
		fprintf(stderr, "mkdir: reqiured operand\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	int retVal = 0;

	for (int i=1; argv[i]; i++) {
		if (argv[i][0] != '-') {
			if (makeParent) {
				create_parent(argv[i], perm);
			} else {
				if (mkdir (argv[i], perm) < 0) {
					perror("mkdir: cannot create directory");
					retVal = 1;
					continue;
				}
				if(verbose) printf("mkdir: created directory '%s'\n", argv[i]);
			}
		}	
	}

	return retVal;
}

REGISTER_MODULE(mkdir);
