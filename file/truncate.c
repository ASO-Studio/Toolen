#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include "config.h"
#include "module.h"

// Function to parse size string with units (K, M, G, etc.)
static off_t parse_size(const char *str) {
	char *endptr;
	double value;
	off_t multiplier = 1;
	
	// Parse numeric value
	value = strtod(str, &endptr);
	if (endptr == str) {
		fprintf(stderr, "Invalid size value: '%s'\n", str);
		exit(EXIT_FAILURE);
	}
	
	// Skip whitespace
	while (*endptr == ' ') endptr++;
	
	// Parse size unit
	if (*endptr != '\0') {
		switch (*endptr) {
			case 'K': case 'k': multiplier = 1024; break;
			case 'M': case 'm': multiplier = 1024 * 1024; break;
			case 'G': case 'g': multiplier = 1024 * 1024 * 1024; break;
			case 'T': case 't': multiplier = 1024LL * 1024 * 1024 * 1024; break;
			case 'P': case 'p': multiplier = 1024LL * 1024 * 1024 * 1024 * 1024; break;
			case 'E': case 'e': multiplier = 1024LL * 1024 * 1024 * 1024 * 1024 * 1024; break;
			default:
				fprintf(stderr, "Invalid size unit: '%c'\n", *endptr);
				exit(EXIT_FAILURE);
		}
		
		// Check for valid suffix
		if (*(endptr + 1) != '\0') {
			fprintf(stderr, "Invalid size suffix: '%s'\n", endptr);
			exit(EXIT_FAILURE);
		}
	}
	
	// Calculate final size
	off_t result = (off_t)(value * multiplier);
	
	// Check for negative size
	if (result < 0) {
		fprintf(stderr, "Size cannot be negative\n");
		exit(EXIT_FAILURE);
	}
	
	return result;
}

// Function to truncate a file to specified size
static int truncate_file(const char *filename, off_t size, bool no_create) {
	int fd;
	struct stat st;
	
	// Check if file exists
	if (stat(filename, &st) < 0) {
		if (errno == ENOENT) {
			// File doesn't exist
			if (no_create) {
				fprintf(stderr, "%s: cannot truncate: No such file or directory\n", filename);
				return -1;
			}
			
			// Create new file
			fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0) {
				perror(filename);
				return -1;
			}
		} else {
			perror(filename);
			return -1;
		}
	} else {
		// File exists, open for writing
		fd = open(filename, O_WRONLY);
		if (fd < 0) {
			perror(filename);
			return -1;
		}
	}
	
	// Truncate file to specified size
	if (ftruncate(fd, size) < 0) {
		perror(filename);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}

static void truncate_show_help() {
	fprintf(stderr, "Usage: truncate -s SIZE [-c] FILE...\n\n"
			"Truncate FILE(s) to the specified SIZE\n\n"
			"Options:\n"
			"  -c, --no-create   do not create any files\n"
			"  -s, --size=SIZE   set file size (e.g., 10K, 5M, 1G)\n"
			"  -h, --help        display this help and exit\n");
}

int truncate_main(int argc, char *argv[]) {
	bool no_create = false;
	off_t size = 0;
	const char *size_str = NULL;
	int opt;
	
	// Parse command-line options
	static struct option long_options[] = {
		{"no-create", no_argument, NULL, 'c'},
		{"size", required_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	
	while ((opt = getopt_long(argc, argv, "cs:h", long_options, NULL)) != -1) {
		switch (opt) {
			case 'c':
				no_create = true;
				break;
			case 's':
				size_str = optarg;
				break;
			case 'h':
				truncate_show_help();
				exit(EXIT_SUCCESS);
			case 'V':
				JUST_VERSION();
				exit(EXIT_SUCCESS);
			default:
				truncate_show_help();
				exit(EXIT_FAILURE);
		}
	}
	
	// Check required options
	if (size_str == NULL) {
		fprintf(stderr, "You must specify a size with -s\n");
		truncate_show_help();
		exit(EXIT_FAILURE);
	}
	
	if (optind >= argc) {
		fprintf(stderr, "Missing file operand\n");
		truncate_show_help();
		exit(EXIT_FAILURE);
	}
	
	// Parse size string
	size = parse_size(size_str);
	
	// Process each file
	int exit_status = EXIT_SUCCESS;
	for (int i = optind; i < argc; i++) {
		if (truncate_file(argv[i], size, no_create) < 0) {
			exit_status = EXIT_FAILURE;
		}
	}
	
	return exit_status;
}

REGISTER_MODULE(truncate);
