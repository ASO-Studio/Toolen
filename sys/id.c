#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <getopt.h>

#include "config.h"
#include "module.h"
#include "lib.h"

// Function to display help information
void print_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: id [OPTION]... [USER]\n\n"
		"Print user and group information for the specified USER,\n"
		"or for the current user if no USER is specified.\n\n"
		"Options:\n"
		"  -a             ignore, for compatibility with other versions\n"
		"  -g, --group    print only the effective group ID\n"
		"  -G, --groups   print all group IDs\n"
		"  -n, --name     print a name instead of a number\n"
		"  -r, --real     print the real ID instead of the effective ID\n"
		"  -u, --user     print only the effective user ID\n"
		"  -h, --help     display this help and exit\n");
}

// Function to print user information
void print_user_info(uid_t uid, int use_name) {
	if (use_name) {
		printf("%s", get_username(uid));
	} else {
		printf("%d", uid);
	}
}

// Function to print group information
void print_group_info(gid_t gid, int use_name) {
	if (use_name) {
		printf("%s", get_groupname(gid));
	} else {
		printf("%d", gid);
	}
}

// Function to print supplementary groups
void print_supplementary_groups(int use_name) {
	gid_t *groups = NULL;
	int ngroups = 0;

	// Get the number of supplementary groups
	ngroups = getgroups(0, NULL);
	if (ngroups < 0) {
		perror("getgroups");
		return;
	}

	// Allocate memory for the group list
	groups = xmalloc(ngroups * sizeof(gid_t));
	
	// Get the supplementary groups
	if (getgroups(ngroups, groups) < 0) {
		perror("getgroups");
		xfree(groups);
		return;
	}

	// Print the supplementary groups
	for (int i = 0; i < ngroups; i++) {
		if (use_name) {
			printf("%s", get_groupname(groups[i]));
		} else {
			printf("%d", groups[i]);
		}

		if (i < ngroups - 1) {
			printf(" ");
		}
	}

	xfree(groups);
}

// Function to get group IDs for a specific user
gid_t* get_user_groups(const char *username, int *count) {
	// This is a simplified implementation
	// In a real implementation, you would parse /etc/group or use other methods
	*count = 1;
	gid_t *groups = xmalloc(sizeof(gid_t));
	groups[0] = getgid_name(username);
	return groups;
}

int id_main(int argc, char *argv[]) {
	int opt;
	int show_user = 0;
	int show_group = 0;
	int show_groups = 0;
	int use_name = 0;
	int use_real = 0;
	int show_help = 0;

	char *username = NULL;

	// Define long options
	struct option long_options[] = {
		{"group", no_argument, 0, 'g'},
		{"groups", no_argument, 0, 'G'},
		{"name", no_argument, 0, 'n'},
		{"real", no_argument, 0, 'r'},
		{"user", no_argument, 0, 'u'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	// Parse command line options
	while ((opt = getopt_long(argc, argv, "agGnruh", long_options, NULL)) != -1) {
		switch (opt) {
			case 'a': // Ignored for compatibility
				break;
			case 'g':
				show_group = 1;
				break;
			case 'G':
				show_groups = 1;
				break;
			case 'n':
				use_name = 1;
				break;
			case 'r':
				use_real = 1;
				break;
			case 'u':
				show_user = 1;
				break;
			case 'h':
				show_help = 1;
				break;
			default:
				fprintf(stderr, "Try 'id --help' for more information.\n");
				return 1;
		}
	}

	// Check for username argument
	if (optind < argc) {
		username = argv[optind];
	}

	// Show help if requested
	if (show_help) {
		print_help();
		return 0;
	}

	// Determine which IDs to use (real or effective)
	uid_t uid = use_real ? getuid() : geteuid();
	gid_t gid = use_real ? getgid() : getegid();

	// If a username is provided, get that user's information
	if (username != NULL) {
		uid = getuid_name(username);
		gid = getgid_name(username);
	}

	// Handle specific output options
	if (show_user) {
		print_user_info(uid, use_name);
		printf("\n");
	} else if (show_group) {
		print_group_info(gid, use_name);
		printf("\n");
	} else if (show_groups) {
		if (username != NULL) {
			// For another user, get their groups
			int group_count = 0;
			gid_t *groups = get_user_groups(username, &group_count);
			
			for (int i = 0; i < group_count; i++) {
				if (use_name) {
					printf("%s", get_groupname(groups[i]));
				} else {
					printf("%d", groups[i]);
				}
				
				if (i < group_count - 1) {
					printf(" ");
				}
			}
			
			xfree(groups);
			printf("\n");
		} else {
			// For current user, use getgroups
			print_supplementary_groups(use_name);
			printf("\n");
		}
	} else {
		// Default output: show uid, gid, and groups
		printf("uid=");
		print_user_info(uid, 0);
		printf("(");
		print_user_info(uid, 1);
		printf(") gid=");
		print_group_info(gid, 0);
		printf("(");
		print_group_info(gid, 1);
		printf(") groups=");

		if (username != NULL) {
			// For another user
			int group_count = 0;
			gid_t *groups = get_user_groups(username, &group_count);
			
			for (int i = 0; i < group_count; i++) {
				if (i > 0) printf(",");
				printf("%d(", groups[i]);
				print_group_info(groups[i], 1);
				printf(")");
			}
			
			xfree(groups);
		} else {
			// For current user
			gid_t *groups = NULL;
			int ngroups = getgroups(0, NULL);
			if (ngroups > 0) {
				groups = xmalloc(ngroups * sizeof(gid_t));
				if (groups != NULL && getgroups(ngroups, groups) >= 0) {
					for (int i = 0; i < ngroups; i++) {
						if (i > 0) printf(",");
						printf("%d(", groups[i]);
						print_group_info(groups[i], 1);
						printf(")");
					}
				}
				xfree(groups);
			}
		}
		printf("\n");
	}

	return 0;
}

REGISTER_MODULE(id);
