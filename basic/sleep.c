/**
 *	exit.c - Wait before exiting
 *
 * 	Created by RoofAlan
 *		2025/8/18
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "module.h"
#include "lib.h"

// Convert string to seconds
static unsigned long toSecond(const char *tm) {
	unsigned long num = 1;	// Default is second, so set it to 1
	switch(tm[strlen(tm) - 1]) {
		case 'm':	// Minute
			num *= 60;
			break;
		case 'h':	// Hour
			num *= 60 * 60;
			break;
		case 'd':	// Day
			num *= 24 * 60 * 60;
			break;
		case 's':
		default:	// Second (the default)
			break;
	}

	char *strTime = strdup(tm);
	if (!strTime) {
		perror("sleep");
		exit(1);
	}

	strTime[strlen(tm)-1 == 0 ? 1 : strlen(tm)-1] = '\0';
	unsigned long ret = strtoul(strTime, NULL, 0);

	return ret * num;
}

// Show help (sleep)
static void show_sleep_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: sleep DURATION ...\n\n"
			"Wait before exiting\n\n");
	fprintf(stderr, "DURATION: decimal[d: day|h: hour|m: minute|s: second] (the second is the default)\n");
}

// Show help (usleep)
static void show_usleep_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: usleep MICRODURATION ...\n\n"
			"Wait before exiting\n");
}

M_ENTRY(sleep) {
	if(argc < 2) {
		pplog(P_NAME | P_HELP, "Need an argument");
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if(strcmp(argv[1], "--help") == 0) {
			show_sleep_help();
			return 0;
		} else if (strcmp(argv[1], "--version") == 0) {
			JUST_VERSION();
			return 0;
		}
		usleep(toSecond(argv[i]) * 1000000); // Get seconds
	}

	return 0;
}

M_ENTRY(usleep) {
	if(argc < 2) {
		pplog(P_NAME | P_HELP, "Need an argument");
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if(strcmp(argv[1], "--help") == 0) {
			show_usleep_help();
			return 0;
		}
		usleep(strtoull(argv[i], NULL, 0));	// Just use strtoull
	}

	return 0;
}

REGISTER_MODULE(sleep);
REGISTER_MODULE(usleep);
