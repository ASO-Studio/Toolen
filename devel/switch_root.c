#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>

#include "config.h"
#include "module.h"

// ANSI color codes for error messages
#define RED	 "\033[1;31m"
#define RESET   "\033[0m"

// pivot_root syscall wrapper
#ifndef SYS_pivot_root
#  if defined(__x86_64__)
#	define SYS_pivot_root 155
#  elif defined(__i386__)
#	define SYS_pivot_root 217
#  elif defined(__aarch64__)
#	define SYS_pivot_root 41
#  else
#	error "Architecture not supported for pivot_root"
#  endif
#endif

// Virtual filesystems to preserve
static const char * const VIRTUAL_FS[] = {
	"/proc", "/sys", "/dev", "/run", NULL
};

/**
 * Print error message in coreutils style
 * 
 * @param prefix Message prefix
 * @param msg Error message
 */
static void error_msg(const char *prefix, const char *msg) {
	fprintf(stderr, RED "%s: " RESET "%s\n", prefix, msg);
}

/**
 * Print system error message
 * 
 * @param context Error context
 */
static void sys_error(const char *context) {
	fprintf(stderr, RED "switch_root: " RESET "%s: %s\n", 
			context, strerror(errno));
}

/**
 * Display help information
 */
static void usage(void) {
	SHOW_VERSION(stdout);
	printf("Usage: switch_root [OPTION]... NEW_ROOT INIT [ARG]...\n");
	printf("Switch from temporary filesystem to real root filesystem.\n\n");
	printf("Mandatory arguments:\n");
	printf("  NEW_ROOT  path to new root directory\n");
	printf("  INIT	  path to init program relative to NEW_ROOT\n");
	printf("  ARG	   arguments passed to INIT program\n\n");
	printf("Options:\n");
	printf("  -h, --help	 display this help and exit\n");
	printf("  -v, --version  output version information and exit\n\n");
	printf("Exit status:\n");
	printf("  0  success\n");
	printf("  1  operation failed\n");
	printf("  2  invalid arguments\n\n");
}

/**
 * Create directory path if it doesn't exist
 * 
 * @param path Directory path to create
 * @param mode Directory permissions
 * @return 0 on success, -1 on failure
 */
static int ensure_dir(const char *path, mode_t mode) {
	struct stat st;
	
	if (stat(path, &st) < 0) {
		if (mkdir(path, mode) < 0) {
			sys_error("mkdir failed");
			return -1;
		}
	} else if (!S_ISDIR(st.st_mode)) {
		error_msg("Path exists but is not a directory", path);
		return -1;
	}
	return 0;
}

/**
 * Move virtual filesystems to new root
 * 
 * @param new_root Path to new root directory
 * @return 0 on success, -1 on failure
 */
static int move_virtual_fs(const char *new_root) {
	for (int i = 0; VIRTUAL_FS[i]; i++) {
		char src[PATH_MAX], dest[PATH_MAX];
		
		// Build source and destination paths
		snprintf(src, sizeof(src), "%s", VIRTUAL_FS[i]);
		snprintf(dest, sizeof(dest), "%s%s", new_root, VIRTUAL_FS[i]);
		
		// Create target directory if missing
		if (ensure_dir(dest, 0755)) {
			return -1;
		}
		
		// Move mount to new location
		if (mount(src, dest, "", MS_MOVE, NULL) < 0) {
			sys_error("mount(MS_MOVE) failed");
			return -1;
		}
	}
	return 0;
}

/**
 * Perform pivot_root operation
 * 
 * @param new_root Path to new root directory
 * @return 0 on success, -1 on failure
 */
static int pivot_root(const char *new_root) {
	char put_old[PATH_MAX];
	
	// Create .oldroot directory for old root
	snprintf(put_old, sizeof(put_old), "%s/.oldroot", new_root);
	if (mkdir(put_old, 0777) && errno != EEXIST) {
		sys_error("mkdir .oldroot failed");
		return -1;
	}

	// Change to new root directory
	if (chdir(new_root)) {
		sys_error("chdir(new_root) failed");
		return -1;
	}

	// Execute pivot_root system call
	if (syscall(SYS_pivot_root, ".", ".oldroot")) {
		sys_error("pivot_root failed");
		return -1;
	}

	// Change to new root in mounted namespace
	if (chdir("/")) {
		sys_error("chdir(/) failed");
		return -1;
	}

	// Unmount old root with lazy detach
	if (umount2("/.oldroot", MNT_DETACH)) {
		sys_error("umount2(.oldroot) failed");
		return -1;
	}

	// Cleanup old root directory
	if (rmdir("/.oldroot")) {
		sys_error("rmdir(.oldroot) failed");
		return -1;
	}

	return 0;
}

/**
 * Execute new init program
 * 
 * @param init_path Path to init program
 * @param argv Argument vector
 */
static void execute_init(const char *init_path, char *argv[]) {
	// Set environment for new init
	clearenv();
	putenv("PATH=/bin:/sbin:/usr/bin:/usr/sbin");
	
	execv(init_path, argv);
	sys_error("execv failed");
	_exit(EXIT_FAILURE);  // Use _exit to avoid flushing stdio buffers
}

int switch_root_main(int argc, char *argv[]) {
	// Parse command-line options
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};
	
	int opt;
	while ((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1 ) {
		if (opt == -1) break;
		
		switch (opt) {
			case 'h':
				usage();
				return EXIT_SUCCESS;
			case 'v':
				JUST_VERSION();
				return EXIT_SUCCESS;
			default:
				usage();
				return 2;
		}
	}
	
	// Validate arguments
	if (argc - optind < 2) {
		error_msg("Insufficient arguments", "");
		usage();
		return 2;
	}
	
	const char *new_root = argv[optind];
	const char *init_cmd = argv[optind + 1];
	
	// Verify new root is a directory
	struct stat st;
	if (stat(new_root, &st)) {
		sys_error("stat(new_root) failed");
		return 1;
	}
	if (!S_ISDIR(st.st_mode)) {
		error_msg("Path is not a directory", new_root);
		return 1;
	}

	// Build absolute path to init program
	char init_path[PATH_MAX];
	if (init_cmd[0] == '/') {
		snprintf(init_path, sizeof(init_path), "%s%s", new_root, init_cmd);
	} else {
		snprintf(init_path, sizeof(init_path), "%s/%s", new_root, init_cmd);
	}
	
	// Verify init program exists
	if (access(init_path, F_OK)) {
		sys_error("Init program not found");
		fprintf(stderr, "  Path: %s\n", init_path);
		return 1;
	}
	
	// Verify init program is executable
	if (access(init_path, X_OK)) {
		sys_error("Init program not executable");
		return 1;
	}

	// Move critical virtual filesystems
	if (move_virtual_fs(new_root)) {
		error_msg("Failed to move virtual filesystems", "");
		return 1;
	}

	// Perform root switch
	if (pivot_root(new_root)) {
		error_msg("Root switching failed", "");
		return 1;
	}

	// Execute new init process
	execute_init(init_path, &argv[optind + 1]);
	
	return 1;  // Should never reach here
}

REGISTER_MODULE(switch_root);
