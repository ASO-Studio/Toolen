/**
 *	fwalk.c - Walk through the dieectory tree that is loacted under the current directory or DIR
 *
 * 	Created by RoofAlan
 *		2025/9/30
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ftw.h>
#include <sys/stat.h>

#include "config.h"
#include "module.h"
#include "lib.h"

#define F_VERBOSE (1 << 0)
#define F_SIZE (1 << 1)

static size_t flags = 0;
static long total_size = 0;

static int display_files(const char *fpath, const struct stat *sb, int typeflag) {
	if (typeflag == FTW_F) {
		if (flags & F_VERBOSE) {
			printf("File: %s Size: %ld bytes\n", fpath, sb->st_size);
			total_size += sb->st_size;
		} else if (flags & F_SIZE) {
			printf("%ld %s\n", sb->st_size, fpath);
		} else {
			printf("%s\n", fpath);
		}
	}
	return 0;
}

static void fwalk_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: fwalk [OPTIONS] [DIR]\n\n"
			"Walk through the dieectory tree that is loacted under the current directory or DIR\n\n"
			"Support options:\n"
			"  -v,--verbose  Verbose output\n"
			"  -s,--size     Print size\n");
}

M_ENTRY(fwalk) {
	int opt;
	int optidx = 0;
	static struct option long_options[] = {
		{"verbose", no_argument, NULL, 'v'},
		{"size", no_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	while ((opt = getopt_long(argc, argv, "vsh", long_options, &optidx)) != -1) {
		switch(opt) {
			case 'v':
				flags |= F_VERBOSE;
				break;
			case 'h':
				fwalk_show_help();
				return 0;
				break;
			case 's':
				flags |= F_SIZE;
				break;
			case '?':
				pplog(P_HELP | P_NAME, "Failed to parse arguments");
				return 1;
				break;
			default:
				abort();
				break;
		}
	}

	const char *dir = NULL;
	for (int i = 1; i < argc; i ++) {
		if (argv[i][0] != '-') {
			dir = argv[i];
			break;
		}
	}

	if (ftw(dir ? : ".", display_files, 10) == -1) {
		pplog(P_NAME | P_ERRNO, dir);
		exit(EXIT_FAILURE);
	}

	if (flags & F_VERBOSE)
		printf("\nTotal size: %ld bytes\n", total_size);
	return 0;
}

REGISTER_MODULE(fwalk);
