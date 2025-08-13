#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <linux/limits.h>
#include <sys/sysmacros.h>

#include "config.h"
#include "module.h"

/* Display error message in coreutils style */
static void error_msg(const char *msg) {
	fprintf(stderr, "%s: %s\n", PROGRAM_NAME, msg);
}

/* Display system error with context */
static void sys_error(const char *context) {
	fprintf(stderr, "mountpoint: %s: %s\n", 
			 context, strerror(errno));
}

/* Display help information */
static void usage(int status) {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: mount_point [OPTION]... DIRECTORY...\n\n"
		"Determine whether directories are mountpoints.\n\n"
		"Support options:\n"
		"  -q, --quiet      suppress output, only return exit status\n"
		"  -d, --devno      print filesystem device numbers (major:minor)\n"
		"  -h, --help       display this help and exit\n");
	exit(status);
}

/**
 * Check if a directory is a mountpoint
 * 
 * @param dir	 Directory path to check
 * @param devno   Output parameter: device number if mountpoint
 * @return		1 if mountpoint, 0 if not, -1 on error
 */
static int is_mountpoint(const char *dir, dev_t *devno) {
	struct stat st_dir, st_parent;
	char parent_path[PATH_MAX];
	
	// Special handling for root directory
	if (strcmp(dir, "/") == 0) {
		if (stat("/", &st_dir) == -1) {
			sys_error("stat(\"/\") failed");
			return -1;
		}
		if (devno) *devno = st_dir.st_dev;
		return 1;  // Root is always a mountpoint
	}
	
	// Get directory information (use lstat to avoid symlink issues)
	if (lstat(dir, &st_dir)) {
		sys_error("lstat failed");
		return -1;
	}
	
	// Build parent directory path
	snprintf(parent_path, sizeof(parent_path), "%s/..", dir);
	
	// Get parent directory information (use stat for correct parent resolution)
	if (stat(parent_path, &st_parent)) {
		sys_error("stat(parent) failed");
		return -1;
	}
	
	// Store device number if requested
	if (devno) *devno = st_dir.st_dev;
	
	// Compare device IDs: different = mountpoint
	return (st_dir.st_dev != st_parent.st_dev) ? 1 : 0;
}

int mountpoint_main(int argc, char *argv[]) {
	int quiet = 0;		  // Quiet mode flag
	int devno_mode = 0;	 // Device number output mode
	int exit_status = 0;	// Program exit status
	
	// Parse command-line options
	struct option long_options[] = {
		{"quiet", no_argument, 0, 'q'},
		{"devno", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{NULL, 0, NULL, 0}
	};
	
	int opt;
	while ((opt = getopt_long(argc, argv, "qdvh", long_options, NULL)) != -1) {
		if (opt == -1) break;
		
		switch (opt) {
		case 'q':
			quiet = 1;  // Enable quiet mode
			break;
		case 'd':
			devno_mode = 1;  // Enable device number output
			break;
		case 'v':
			JUST_VERSION();   // Show version
			return EXIT_SUCCESS;
		case 'h':
			usage(EXIT_SUCCESS);  // Show help
		default:
			usage(EXIT_FAILURE);  // Invalid option
		}
	}
	
	// Validate arguments
	if (optind >= argc) {
		error_msg("missing operand");
		usage(EXIT_FAILURE);
	}
	
	// Process each directory argument
	for (int i = optind; i < argc; i++) {
		const char *dir = argv[i];
		dev_t dev = 0;
		int result = is_mountpoint(dir, &dev);
		
		if (result < 0) {
			// Error occurred
			if (!quiet) sys_error(dir);
			// Set exit status to 2 (error) if not already set
			if (exit_status != 2) exit_status = 2;
		} else {
			if (result == 1) {
				// Directory is a mountpoint
				if (!quiet) {
					if (devno_mode) {
						// Print device number in major:minor format
						printf("%u:%u\n", major(dev), minor(dev));
					} else {
						printf("%s is a mountpoint\n", dir);
					}
				}
				// Only update status if no previous error
				if (exit_status == 0) exit_status = 0;
			} else {
				// Directory is not a mountpoint
				if (!quiet && !devno_mode) {
					printf("%s is not a mountpoint\n", dir);
				}
				// Update status to 1 if no previous failure
				if (exit_status == 0) exit_status = 1;
			}
		}
	}
	
	return exit_status;
}

REGISTER_MODULE(mountpoint);
