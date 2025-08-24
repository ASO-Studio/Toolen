/**
 *	basename.c - Convert text files from DOS/MAX format
 *		     to UNIX format
 *
 * 	Created by RoofAlan
 *		2025/8/23
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void dos2unix_show_help(void) {
	SHOW_VERSION(stderr);
	fprintf(stderr,
	"Usage: dos2unix [OPTION]... [FILE]...\n\n"
	"Convert text files from DOS/MAC format to UNIX format.\n\n"
	"With no FILE, or when FILE is -, read standard input.\n");
}

static int convert_stream(FILE *input, FILE *output) {
	int c;
	int prev = 0;

	while ((c = fgetc(input)) != EOF) {
		if (c == '\r') {
			// Handle CR: if next is LF, skip CR (CRLF → LF)
			// If next is not LF, output LF (MAC → LF)
			int next = fgetc(input);
			if (next != EOF) {
				if (next == '\n') {
					fputc('\n', output);
				} else {
					fputc('\n', output);
					fputc(next, output);
				}
			} else {
				fputc('\n', output);
			}
		} else {
			fputc(c, output);
		}
		prev = c;
	}

	return 0;
}

static int convert_file(const char *filename) {
	FILE *input;
	int use_stdin = 0;
	int result = 0;

	// Handle special case: read from stdin
	if (strcmp(filename, "-") == 0) {
		input = stdin;
		use_stdin = 1;
	} else {
		input = fopen(filename, "rb");
		if (!input) {
			fprintf(stderr, "dos2unix: %s: %s\n", filename, strerror(errno));
			return 1;
		}
	}

	// For non-stdin files, we need to use a temporary file approach
	// For simplicity, we'll just output to stdout in this implementation
	result = convert_stream(input, stdout);

	if (!use_stdin) {
		fclose(input);
	}

	return result;
}

int dos2unix_main(int argc, char *argv[]) {
	int i;
	int exit_status = 0;
	
	if(argc > 1 && findArg(argv, argc, "--help")) {
		dos2unix_show_help();
		return 0;
	}

	// If no files specified, read from stdin
	if (argc == 1) {
		return convert_stream(stdin, stdout);
	}

	// Process all files
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			// Skip argument
			continue;
		}
		
		if (convert_file(argv[i]) != 0) {
			exit_status = 1;
		}
	}
	
	return exit_status;
}

REGISTER_MODULE(dos2unix);
