/**
 *	passgen.c - Generate human-readable passwords
 *
 * 	Created by RoofAlan
 *		2025/10/4
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static size_t len = 10;
static size_t num = 25;
static int prefix = 0;

static char* randomStr() {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	srand((unsigned int)(ts.tv_sec ^ ts.tv_nsec // Time
				^ (size_t)(&len) // KALSR
				^ getpid() // PID
			));

	const char* sou = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
	int sourceLen = strlen(sou);
	char *randomString = (char *)xmalloc((len + 1) * sizeof(char));

	// Generate random string
	for(size_t i = 0; i < len; i++) {
		int index = rand() % sourceLen;
		randomString[i] = sou[index];
	}
	randomString[len] = '\0';
	return randomString;
}

static int display_passwords() {
	// Get terminal size
	int max_x = 0, max_y = 0;
	if (getTerminalSize(&max_x, &max_y) < 0) {
		pplog(P_NAME | P_ERRNO, "getTerminalSize()");
		return 1;
	}

	// Calculate how many passwords can fit per line
	int col_width = prefix ? (len + 5) : (len + 1); // +5 for "XX: " prefix
	int num_per_line = max_x / col_width;
	num_per_line = num_per_line == 0 ? 1 : num_per_line;

	size_t count = 0;
	while (count < num) {
		for (int i = 0; i < num_per_line; i++) {
			if (count >= num) break;

			count++;
			if (prefix) {
				printf("%-3zu: ", count);
			}

			char *s = randomStr();
			printf("%s", s);
			xfree(s);

			// Add space unless it's the last item in line
			if (i < num_per_line - 1) {
				printf(" ");
			}
			if (prefix) {
				printf("\n");
			}
		}
		if (!prefix) {
			printf("\n");
		}
	}

	return 0;
}

static void passgen_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: passgen [OPTIONS]\n\n"
			"Generate human-readable passwords\n\n"
			"Support options:\n"
			"  -l,--length NUM  Set the passwords length(default=10)\n"
			"  -n,--number NUM  Set the number of the passwords(default=50)\n"
			"  -p,--prefix	  Print prefix\n");
}

M_ENTRY(passgen) {
	int opt;
	int optidx = 0;
	static struct option long_options[] = {
		{"length", required_argument, NULL, 'l'},
		{"number", required_argument, NULL, 'n'},
		{"prefix", no_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0},
	};

	while((opt = getopt_long(argc, argv, "l:n:ph", long_options, &optidx)) != -1) {
		switch(opt) {
			case 'l':
				len = strtoul(optarg, NULL, 0);
				break;
			case 'n':
				num = strtoul(optarg, NULL, 0);
				break;
			case 'p':
				prefix = 1;
				break;
			case 'h':
				passgen_show_help();
				return 0;
				break;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
			default:
				abort();
				break;
		}
	}

	if (num == 0) {
		pplog(P_NAME, "Number must > 0!");
		return 1;
	}
	if (len < 5) {
		pplog(P_NAME, "Length must > 5!");
		return 1;
	}

	display_passwords();

	return 0;
}

REGISTER_MODULE(passgen);
