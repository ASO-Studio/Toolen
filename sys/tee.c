/**
 *	tee.c - Copy stdin to each listed files
 *
 * 	Created by RoofAlan
 *		2025/8/15
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>

#include "config.h"
#include "module.h"
#include "lib.h"

// Show help page
static void tee_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: tee [Options] [FILE] ...\n\n"
			"Copy stdin to each listed files\n\n"
			"Support options:\n"
			"  -a, --append			Append to files\n"
			"  -i, --ignore-interrupts	Ignore SIGINT\n");
}

// Main function
int tee_main(int argc, char *argv[]) {
	int append = 0;
	int opt;

	// Parse arguments
	while ((opt = getopt(argc, argv, "aiVh")) != -1) {
		switch (opt) {
			case 'a': append = 1; break;
			case 'i': signal(SIGINT, SIG_IGN); break;
			case 'h': tee_show_help(); return 0; break;
			default: 
				fprintf(stderr, "Usage: %s [-ai] [file...]\n", argv[0]);
				fprintf(stderr, "Try pass '--help' for more details\n");
				exit(EXIT_FAILURE);
		}
	}

	// Open all files
	int *fds = xmalloc((argc - optind + 1) * sizeof(int));

	// Open STDOUT
	fds[0] = STDOUT_FILENO;
	int fd_count = 1;

	// Open files by user gave
	for (int i = optind; i < argc; i++) {
		int flags = O_WRONLY | O_CREAT;
		if (append) flags |= O_APPEND;
		else flags |= O_TRUNC;
		
		fds[fd_count] = open(argv[i], flags, 0644);
		if (fds[fd_count] < 0) {
			perror(argv[i]);
			continue;	// Countine porcess file
		}
		fd_count++;
	}

	// Read input and write to output
	char buffer[4096];
	ssize_t bytes_read;

	while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		if (bytes_read < 0) {
			perror("read");
			break;
		}
		
		// Write all outputs
		for (int i = 0; i < fd_count; i++) {
			ssize_t bytes_written = write(fds[i], buffer, bytes_read);
			if (bytes_written < 0) {
				perror("write");
			}
		}
	}
	
	// Close all file descripters
	for (int i = 1; i < fd_count; i++) {
		close(fds[i]);
	}
	
	xfree(fds);
	return EXIT_SUCCESS;
}

REGISTER_MODULE(tee);
