/**
 *	cat.c - Copy(concatenate) files to stdout
 *
 *	Created by RoofAlan
 *		2025/8/18
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "module.h"

static bool markWithD = false;
static bool tabAsI = false;

// Display help information
static void cat_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: cat [options] [FILE]...\n\n"
			"Copy (concatenate) files to stdout. If no files given, copy from stdin\n\n"
			"Supported options:\n"
			"  -e   Mark each newline with $\n"
			"  -t   Show tabs as ^I\n");
}

// Handle reading from special character devices
static void cat_special_device(int fd) {
	// Set non-blocking mode
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	char buffer[4096];
	ssize_t bytes_read;

	while (1) {
		bytes_read = read(fd, buffer, sizeof(buffer) - 1);
		if (bytes_read < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				usleep(100000); // 100ms delay
				continue;
			}
			perror("read");
			break;
		}

		if (bytes_read == 0) {
			break; // EOF
		}

		// Null-terminate the buffer
		buffer[bytes_read] = '\0';

		// Process each message line
		char *line = buffer;
		while (line < buffer + bytes_read) {
			char *end = strchr(line, '\n');
			if (!end) {
				// No newline found, write remaining content
				write(1, line, strlen(line));
				break;
			}

			// Calculate message length
			size_t len = end - line;

			// Handle special markers
			if (markWithD) {
				write(1, line, len);
				write(1, "$\n", 2);
			} else if (tabAsI) {
				for (size_t i = 0; i < len; i++) {
					if (line[i] == '\t') {
						write(1, "^I", 2);
					} else {
						write(1, &line[i], 1);
					}
				}
				write(1, "\n", 1);
			} else {
				write(1, line, len);
				write(1, "\n", 1);
			}

			// Move to next message
			line = end + 1;
		}
	}
}

// Handle reading regular files
static void cat_regular_file(int fd) {
	lseek(fd, 0, SEEK_SET);
	char ch[2];
	int readRet = 0;
	while(1) {
		if((readRet = read(fd, ch, 1)) <= 0) {
			break;
		}
		if(markWithD && ch[0] == '\n') {
			write(1, "$", 1);
		}
		if(tabAsI && ch[0] == '\t') {
			write(1, "^I", 2);
			continue;
		}
		write(1, ch, 1);
	}
}

// Main function
M_ENTRY(cat) {
	int opt;
	int optidx = 0;
	struct option opts[] = {
		{"e", no_argument, 0, 'e'},
		{"t", no_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	// Parse command line arguments
	while((opt = getopt_long(argc, argv, "eth", opts, &optidx)) != -1) {
		switch(opt) {
			case 'e':
				markWithD = true;
				break;
			case 't':
				tabAsI = true;
				break;
			case 'h':
				cat_show_help();
				return 0;
			case '?':
				fprintf(stderr, "Try '--help' for more details\n");
				return 1;
			default:
				abort();
		}
	}

	// If no files specified, read from stdin
	if(argc - optind <= 0) {
		cat_regular_file(0);
		return 0;
	}

	int ret = 0;
	for(int i = optind; i < argc; i++) {
		if(argv[i][0] != '-') {
			int fd = open(argv[i], O_RDONLY);
			if(fd < 0) {
				perror(argv[i]);
				ret = 1;
				continue;
			}

			struct stat st;
			if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
				cat_special_device(fd);
			} else {
				posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
				cat_regular_file(fd);
			}

			close(fd);
		}
	}

	return ret;
}

REGISTER_MODULE(cat);
