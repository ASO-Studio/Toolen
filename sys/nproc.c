/**
 *	nproc.c - Print the number of processing units available
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "module.h"

static void nproc_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: nproc [OPTION]...\n\n"
			"Print the number of processing units available\n\n"
			"Support options:\n"
			"  --all	  Print the number of installed processors\n"
			"  --ignore=N If possible, exclude N processing units\n"
			"  --help	 Show this help\n");
}

// Get number of processors using sysconf (most portable)
static long get_nproc_sysconf(int all) {
	return sysconf(all ? _SC_NPROCESSORS_CONF : _SC_NPROCESSORS_ONLN);
}

// Get number of processors by counting CPU directories in /sys
static long get_nproc_sysfs() {
	DIR *dir;
	struct dirent *entry;
	long count = 0;
	
	dir = opendir("/sys/devices/system/cpu");
	if (!dir) {
		return -1;
	}
	
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "cpu", 3) == 0 && 
			strspn(entry->d_name + 3, "0123456789") == strlen(entry->d_name + 3)) {
			count++;
		}
	}
	
	closedir(dir);
	return count;
}

// Get number of processors by parsing /proc/cpuinfo
static long get_nproc_procfs() {
	FILE *fp;
	char line[256];
	long count = 0;
	
	fp = fopen("/proc/cpuinfo", "r");
	if (!fp) {
		return -1;
	}
	
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "processor", 9) == 0) {
			count++;
		}
	}
	
	fclose(fp);
	return count;
}

M_ENTRY(nproc) {
	int all = 0;
	long ignore = 0;
	long nproc;
	
	// Parse command line options
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--all") == 0) {
			all = 1;
		} else if (strncmp(argv[i], "--ignore=", 9) == 0) {
			ignore = strtol(argv[i] + 9, NULL, 10);
			if (ignore < 0) {
				fprintf(stderr, "nproc: invalid number '%s'\n", argv[i] + 9);
				return 1;
			}
		} else if (strcmp(argv[i], "--help") == 0) {
			nproc_show_help();
			return 0;
		} else {
			fprintf(stderr, "nproc: unrecognized option '%s'\n", argv[i]);
			nproc_show_help();
			return 1;
		}
	}
	
	// Try different methods to get processor count
	nproc = get_nproc_sysconf(all);
	if (nproc == -1) {
		nproc = get_nproc_sysfs();
	}
	if (nproc == -1) {
		nproc = get_nproc_procfs();
	}
	
	if (nproc == -1) {
		fprintf(stderr, "nproc: cannot determine number of processors\n");
		return 1;
	}
	
	// Apply ignore option
	if (ignore > 0) {
		nproc = nproc - ignore;
		if (nproc < 1) {
			nproc = 1;
		}
	}
	
	printf("%ld\n", nproc);
	return 0;
}

REGISTER_MODULE(nproc);
