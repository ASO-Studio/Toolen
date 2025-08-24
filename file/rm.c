/**
 *	rm.c - Delete files
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "config.h"
#include "module.h"

static void rm_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: rm [OPTIONS] FILE...\n\n"
			"Delete files\n\n"
			"Support options:\n"
			"  -f   Delete files without error(Except that the file exists but the deletion fails)\n"
			"  -i   Confirm before deleting\n"
			"  -rR  Chain deletion\n"
			"  -v   Verbose\n");
}

// Function to check if a file is read-only (no write permission for user)
static int is_readonly(const char *path) {
	struct stat st;
	if (stat(path, &st) == -1) {
		return 0; // If we can't stat, assume not read-only
	}
	return (st.st_mode & S_IWUSR) == 0; // Check if user write permission is missing
}

// Function to prompt user for confirmation
static int prompt_user(const char *message) {
	printf("%s", message);
	char response[4];
	if (fgets(response, sizeof(response), stdin) == NULL) {
		return 0; // Assume no on error
	}
	return (response[0] == 'y' || response[0] == 'Y');
}

// Recursively remove directory contents
static int remove_directory(const char *path, int force, int interactive, int verbose) {
	DIR *dir = opendir(path);
	if (!dir) {
		if (!force) perror(path);
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		char full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

		struct stat st;
		if (lstat(full_path, &st) == -1) {
			if (!force) perror(full_path);
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			remove_directory(full_path, force, interactive, verbose);
		} else {
			int should_remove = 1;
			
			// Check if we need to prompt for read-only files
			if (!force && is_readonly(full_path)) {
				char prompt_msg[1024];
				snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove write-protected regular file '%s'? ", full_path);
				should_remove = prompt_user(prompt_msg);
			}
			
			// Check if we need to prompt for interactive mode
			if (should_remove && interactive) {
				char prompt_msg[1024];
				snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove '%s'? ", full_path);
				should_remove = prompt_user(prompt_msg);
			}
			
			if (should_remove || force) {
				if (remove(full_path) != 0) {
					if (!force) perror(full_path);
				} else if (verbose) {
					printf("removed '%s'\n", full_path);
				}
			}
		}
	}
	closedir(dir);

	int should_remove = 1;
	
	// Check if we need to prompt for read-only directory
	if (!force && is_readonly(path)) {
		char prompt_msg[1024];
		snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove write-protected directory '%s'? ", path);
		should_remove = prompt_user(prompt_msg);
	}
	
	// Check if we need to prompt for interactive mode
	if (should_remove && interactive) {
		char prompt_msg[1024];
		snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove directory '%s'? ", path);
		should_remove = prompt_user(prompt_msg);
	}
	
	if (should_remove || force) {
		if (rmdir(path) != 0) {
			if (!force) perror(path);
			return -1;
		} else if (verbose) {
			printf("removed directory '%s'\n", path);
		}
	}
	return 0;
}

int rm_main(int argc, char *argv[]) {
	int opt;
	int force = 0, interactive = 0, recursive = 0, verbose = 0;

	// Parse command line options
	while ((opt = getopt(argc, argv, "firRv")) != -1) {
		switch (opt) {
			case 'f': force = 1; break;
			case 'i': interactive = 1; break;
			case 'r': case 'R': recursive = 1; break;
			case 'v': verbose = 1; break;
			default:
				rm_show_help();
				return 1;
		}
	}

	// Check if files are provided
	if (optind >= argc) {
		rm_show_help();
		return 1;
	}

	int ret = 0;
	for (int i = optind; i < argc; i++) {
		struct stat st;
		if (lstat(argv[i], &st) == -1) {
			if (!force) {
				perror(argv[i]);
				ret = 1;
			}
			continue;
		}

		if (S_ISDIR(st.st_mode) && !recursive) {
			fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", argv[i]);
			ret = 1;
			continue;
		}

		int should_remove = 1;
		
		// Check if we need to prompt for read-only files
		if (!force && is_readonly(argv[i])) {
			char prompt_msg[1024];
			snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove write-protected regular file '%s'? ", argv[i]);
			should_remove = prompt_user(prompt_msg);
		}
		
		// Check if we need to prompt for interactive mode
		if (should_remove && interactive) {
			char prompt_msg[1024];
			snprintf(prompt_msg, sizeof(prompt_msg), "rm: remove '%s'? ", argv[i]);
			should_remove = prompt_user(prompt_msg);
		}

		if (S_ISDIR(st.st_mode)) {
			if (remove_directory(argv[i], force, interactive, verbose) != 0) {
				ret = 1;
			}
		} else if (should_remove || force) {
			if (remove(argv[i]) != 0) {
				if (!force) {
					perror(argv[i]);
					ret = 1;
				}
			} else if (verbose) {
				printf("removed '%s'\n", argv[i]);
			}
		}
	}

	return ret;
}

REGISTER_MODULE(rm);
