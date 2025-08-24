/**
 *	mkfifo.c - Create FIFOs (named pipes)
 *
 * 	Created by RoofAlan
 *		2025/8/18
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "module.h"

// Show help
static void mkfifo_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: mkfifo [NAME]...\n\n"
			"Create FIFOs (named pipes)\n");
}

// Main functions
int mkfifo_main(int argc, char *argv[]) {
	if(!argv[1]) {
		fprintf(stderr, "mkfifo: no argument gave\n"
				"Try pass '--help' for more details\n");
	} else if (strcmp(argv[1], "--help") == 0) {
		mkfifo_show_help();
		return 0;
	}

	// Default mode: 0666 (-rw-rw-rw-), real: prw-rw-r--
	mode_t mode = 0666;
	for(int i = 1; i < argc; i++) {
		if (mkfifo(argv[i], mode) < 0) {
			perror("mkfifo");	// Print error informations when failed to create pipe
			return 1;
		}
	}

	return 0;
}

REGISTER_MODULE(mkfifo);
