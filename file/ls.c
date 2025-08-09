#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <getopt.h>

#include "module.h"
#include "config.h"

// Convert file type and permission to string
const char *file_type(mode_t mode) {
	if (S_ISREG(mode)) return "-";
	if (S_ISDIR(mode)) return "d";
	if (S_ISCHR(mode)) return "c";
	if (S_ISBLK(mode)) return "b";
	if (S_ISFIFO(mode)) return "p";
	if (S_ISLNK(mode)) return "l";
	if (S_ISSOCK(mode)) return "s";
	return "?";
}

const char *permission_str(mode_t mode) {
	static char str[10];
	strcpy(str, "---------");
	
	if (mode & S_IRUSR) str[0] = 'r';
	if (mode & S_IWUSR) str[1] = 'w';
	if (mode & S_IXUSR) str[2] = (mode & S_ISUID) ? 's' : 'x';
	
	if (mode & S_IRGRP) str[3] = 'r';
	if (mode & S_IWGRP) str[4] = 'w';
	if (mode & S_IXGRP) str[5] = (mode & S_ISGID) ? 's' : 'x';
	
	if (mode & S_IROTH) str[6] = 'r';
	if (mode & S_IWOTH) str[7] = 'w';
	if (mode & S_IXOTH) str[8] = (mode & S_ISVTX) ? 't' : 'x';
	
	return str;
}

// Get user name
const char *get_username(uid_t uid) {
	struct passwd *pw = getpwuid(uid);
	return pw ? pw->pw_name : "unknown";
}

// Get group name
const char *get_groupname(gid_t gid) {
	struct group *gr = getgrgid(gid);
	return gr ? gr->gr_name : "unknown";
}

// Get terminal width
int get_terminal_width() {
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		return 80; // Default width
	}
	return w.ws_col;
}

// 颜色定义
#define COLOR_RESET   "\033[0m"
#define COLOR_DIR	 "\033[1;34m"   // Blue
#define COLOR_LINK	"\033[1;36m"   // Cyan
#define COLOR_EXEC	"\033[1;32m"   // Green
#define COLOR_SOCKET  "\033[1;35m"   // Purple
#define COLOR_PIPE	"\033[33m"	 // Yellow
#define COLOR_BLOCK   "\033[1;33m"   // Light yellow
#define COLOR_CHAR	"\033[1;33m"   // Light yello
#define COLOR_ORPHAN  "\033[1;31m"   // Red (Broken links)

// Get color from file types
const char *get_file_color(mode_t mode, const char *path) {
	if (S_ISDIR(mode)) return COLOR_DIR;
	if (S_ISLNK(mode)) {
		// Check it's a valid link
		struct stat st;
		if (stat(path, &st) == -1) {
			return COLOR_ORPHAN; // Broken
		}
		return COLOR_LINK;
	}
	if (S_ISREG(mode)) {
		if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
			return COLOR_EXEC; // 可执行文件
		}
	}
	if (S_ISSOCK(mode)) return COLOR_SOCKET;
	if (S_ISFIFO(mode)) return COLOR_PIPE;
	if (S_ISBLK(mode)) return COLOR_BLOCK;
	if (S_ISCHR(mode)) return COLOR_CHAR;
	return "";
}

// Convert file size to human readable
const char *human_readable(off_t size, bool use_human) {
	static char buffer[32];
	
	if (!use_human) {
		snprintf(buffer, sizeof(buffer), "%lld", (long long)size);
		return buffer;
	}
	
	const char *units[] = {"B", "K", "M", "G", "T", "P", "E", "Z", "Y"};
	int unit_index = 0;
	double display_size = (double)size;
	
	while (display_size >= 1024 && unit_index < (int)(sizeof(units) / sizeof(units[0])) - 1) {
		display_size /= 1024;
		unit_index++;
	}
	
	// Determine the accuracy according to the size
	if (unit_index == 0) {
		snprintf(buffer, sizeof(buffer), "%lld", (long long)display_size);
	} else if (display_size < 10) {
		snprintf(buffer, sizeof(buffer), "%.1f%s", display_size, units[unit_index]);
	} else {
		snprintf(buffer, sizeof(buffer), "%.0f%s", display_size, units[unit_index]);
	}
	
	return buffer;
}

// Struct of options
typedef struct {
	bool list;	  // -l
	bool all;	   // -a
	bool color;	 // --color
	bool unsorted;  // -f
	bool directory; // Whether to display the directory itself
	bool human;	 // -h: human readable file size
} Options;

// Struct of file
typedef struct {
	char *name;
	struct stat st;
	char *path; // Full path
} FileInfo;

// Compare string
int compare_files(const void *a, const void *b) {
	const FileInfo *fa = (const FileInfo *)a;
	const FileInfo *fb = (const FileInfo *)b;
	return strcmp(fa->name, fb->name);
}

// Displays long-form information
void display_long_format(FileInfo *files, int count, Options opts) {
	// Calculate the maximum width for alignment
	int max_nlink = 0, max_size = 0, max_user = 0, max_group = 0;
	int max_size_chars = 0; // Human-readable size character length
	long total_blocks = 0;
	
	// Collect the maximum value
	for (int i = 0; i < count; i++) {
		if (files[i].st.st_nlink > max_nlink) max_nlink = files[i].st.st_nlink;
		
		// Original size
		if (files[i].st.st_size > max_size) max_size = files[i].st.st_size;
		
		const char *user = get_username(files[i].st.st_uid);
		const char *group = get_groupname(files[i].st.st_gid);
		
		if (strlen(user) > max_user) max_user = strlen(user);
		if (strlen(group) > max_group) max_group = strlen(group);
		
		total_blocks += files[i].st.st_blocks;
	}
	
	// Calculate the maximum width of a human-readable size
	if (opts.human) {
		for (int i = 0; i < count; i++) {
			const char *hr = human_readable(files[i].st.st_size, true);
			int len = strlen(hr);
			if (len > max_size_chars) max_size_chars = len;
		}
	} else {
		// Calculate the maximum width represented by the original size string
		char tmp[32];
		snprintf(tmp, sizeof(tmp), "%lld", (long long)max_size);
		max_size_chars = strlen(tmp);
	}
	
	// Convert to string length
	char nlink_buf[20];
	sprintf(nlink_buf, "%d", max_nlink);
	int nlink_width = strlen(nlink_buf);
	
	// Total number of blocks printed (if there are multiple files)
	if (count > 1) {
		printf("total %ld\n", total_blocks / 2);
	}
	
	for (int i = 0; i < count; i++) {
		struct stat *st = &files[i].st;
		const char *name = files[i].name;
		
		// File type and permission
		printf("%s%s ", file_type(st->st_mode), permission_str(st->st_mode));
		
		// Numbers of hard-link
		printf("%*d ", nlink_width, (int)st->st_nlink);
		
		// Owner
		const char *user = get_username(st->st_uid);
		printf("%-*s ", max_user, user);
		
		// Group
		const char *group = get_groupname(st->st_gid);
		printf("%-*s ", max_group, group);
		
		// Size
		const char *size_str = human_readable(st->st_size, opts.human);
		if (opts.human) {
			// Human-readable
			printf("%*s ", max_size_chars, size_str);
		} else {
			// Original size
			printf("%*s ", max_size_chars, size_str);
		}
		
		// Change time
		char time_buf[20];
		struct tm *tm = localtime(&st->st_mtime);
		strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm);
		printf("%s ", time_buf);
		
		// File name
		if (opts.color) {
			const char *color = get_file_color(st->st_mode, files[i].path);
			printf("%s%s%s", color, name, COLOR_RESET);
		} else {
			printf("%s", name);
		}
		
		// If it's a symbolic link, show the target
		if (S_ISLNK(st->st_mode)) {
			char link_target[PATH_MAX];
			ssize_t len = readlink(files[i].path, link_target, sizeof(link_target) - 1);
			if (len != -1) {
				link_target[len] = '\0';
				printf(" -> %s", link_target);
			}
		}
		
		printf("\n");
	}
}

// Show simple format
void display_simple_format(FileInfo *files, int count, Options opts) {
	int max_len = 0;
	for (int i = 0; i < count; i++) {
		int len = strlen(files[i].name);
		if (len > max_len) max_len = len;
	}
	
	int term_width = get_terminal_width();
	int col_width = max_len + 2; // Column
	int num_cols = term_width / col_width;
	if (num_cols == 0) num_cols = 1;
	
	int num_rows = (count + num_cols - 1) / num_cols;
	
	for (int row = 0; row < num_rows; row++) {
		for (int col = 0; col < num_cols; col++) {
			int idx = row + col * num_rows;
			if (idx >= count) continue;
			
			const char *name = files[idx].name;
			if (opts.color) {
				printf("%s%-*s%s", 
					   get_file_color(files[idx].st.st_mode, files[idx].path),
					   max_len, name, COLOR_RESET);
			} else {
				printf("%-*s", max_len, name);
			}
			
			// Spaces between columns (except for the last column)
			if (col < num_cols - 1) {
				printf("  ");
			}
		}
		printf("\n");
	}
}

// List the contents of the catalog
void list_directory(const char *path, Options opts) {
	DIR *dir = opendir(path);
	if (!dir) {
		perror("ls");
		return;
	}
	
	// If it is the directory itself, print the directory name
	if (opts.directory) {
		printf("%s:\n", path);
	}
	
	struct dirent *entry;
	FileInfo *files = NULL;
	int count = 0;
	int capacity = 0;
	
	// Get contents of the catalog
	while ((entry = readdir(dir)) != NULL) {
		// Skip hidden file
		if (!opts.all && entry->d_name[0] == '.') {
			continue;
		}
		
		// Allocate memory
		if (count >= capacity) {
			capacity = capacity ? capacity * 2 : 16;
			files = realloc(files, capacity * sizeof(FileInfo));
		}
		
		// Create full path
		char *full_path = NULL;
		if (asprintf(&full_path, "%s/%s", path, entry->d_name) == -1) {
			perror("asprintf");
			continue;
		}
		
		// Get file information
		FileInfo *fi = &files[count];
		if (lstat(full_path, &fi->st) == -1) {
			perror("lstat");
			free(full_path);
			continue;
		}
		
		fi->name = strdup(entry->d_name);
		fi->path = full_path; // Storage full path
		
		count++;
	}
	closedir(dir);
	
	// Sort
	if (!opts.unsorted) {
		qsort(files, count, sizeof(FileInfo), compare_files);
	}
	
	// Show files
	if (opts.list) {
		display_long_format(files, count, opts);
	} else {
		display_simple_format(files, count, opts);
	}
	
	// Clean up
	for (int i = 0; i < count; i++) {
		free(files[i].name);
		free(files[i].path);
	}
	free(files);
}

// Show help informations
static void print_help() {
	SHOW_VERSION(stderr);
	printf("Usage: ls [OPTION]... [FILE]...\n");
	printf("List information about the FILEs (the current directory by default).\n");
	printf("\n");
	printf("Mandatory arguments to long options are mandatory for short options too.\n");
	printf("  -a, --all		   do not ignore entries starting with .\n");
	printf("  -l				  use a long listing format\n");
	printf("  -f				  do not sort, enable -a\n");
	printf("  -h, --human-readable  with -l, print human readable sizes\n");
	printf("	  --color[=WHEN]   colorize the output; WHEN can be 'always', 'auto', or 'never';\n");
	printf("						 default is 'auto'\n");
	printf("	  --help		  display this help and exit\n");
}

int ls_main(int argc, char *argv[]) {
	Options opts = {0};
	char **paths = NULL;
	int path_count = 0;
	int path_capacity = 0;
	
	// Options
	static struct option long_options[] = {
		{"all", no_argument, NULL, 'a'},
		{"help", no_argument, NULL, 'H'},
		{"human-readable", no_argument, NULL, 'r'},
		{"color", optional_argument, NULL, 'c'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	
	const char* short_options = "alHrfc:h?v";
	
	// Parse command line arguments
	int opt;
	int option_index = 0;
	bool help_requested = false;
	int ret_value = 0;
	
	while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (opt) {
			case 'a':
				opts.all = true;
				break;
				
			case 'l':
				opts.list = true;
				break;
				
			case 'f':
				opts.unsorted = true;
				opts.all = true; // -f
				break;
				
			case 'r': // --human-readable or -r
			case 'h': // -h
				opts.human = true;
				break;
				
			case 'c': // --color
				if (optarg) {
					if (strcmp(optarg, "always") == 0) {
						opts.color = true;
					} else if (strcmp(optarg, "never") == 0) {
						opts.color = false;
					} else if (strcmp(optarg, "auto") == 0) {
						opts.color = isatty(STDOUT_FILENO);
					} else {
						fprintf(stderr, "ls: invalid argument '%s' for --color\n", optarg);
						fprintf(stderr, "Valid arguments are: 'always', 'never', 'auto'\n");
						exit(EXIT_FAILURE);
					}
				} else {
					// 默认行为：--color 等同于 --color=auto
					opts.color = isatty(STDOUT_FILENO);
				}
				break;
				
			case 'H': // --help
				help_requested = true;
				break;
			case 'V': // --version
				JUST_VERSION();
				return 0;

			case '?':
				// Unknown option
				print_help();
				exit(EXIT_FAILURE);
				
			default:
				fprintf(stderr, "ls: invalid option\n");
				print_help();
				exit(EXIT_FAILURE);
		}
	}
	
	// Process help and version requests
	if (help_requested) {
		print_help();
		exit(EXIT_SUCCESS);
	}

	for (int i = optind; i < argc; i++) {
		// Add path
		if (path_count >= path_capacity) {
			path_capacity = path_capacity ? path_capacity * 2 : 4;
			paths = realloc(paths, path_capacity * sizeof(char *));
		}
		paths[path_count++] = argv[i];
	}
	

	if (path_count == 0) {
		path_count = 1;
		paths = malloc(sizeof(char *));
		paths[0] = ".";
	}
	
	// Process multiple paths
	for (int i = 0; i < path_count; i++) {
		struct stat st;
		if (stat(paths[i], &st) == -1) {
			fprintf(stderr, "ls: cannot access '%s': ", paths[i]);
			perror("");
			ret_value = 2;
			continue;
		}
		
		if (S_ISDIR(st.st_mode)) {
			// If it is a direcory
			opts.directory = (path_count > 1);
			list_directory(paths[i], opts);
			if (i < path_count - 1) {
				printf("\n");
			}
		} else {
			// If it is a file
			FileInfo file;
			// Copy file name
			file.name = strdup(basename(strdup(paths[i])));
			file.path = strdup(paths[i]);
			
			if (lstat(paths[i], &file.st) == -1) {
				fprintf(stderr, "ls: cannot access '%s': ", paths[i]);
				perror("");
				free(file.name);
				free(file.path);
				continue;
			}
			
			if (opts.list) {
				display_long_format(&file, 1, opts);
			} else {
				if (opts.color) {
					printf("%s%s%s\n", get_file_color(file.st.st_mode, paths[i]), file.name, COLOR_RESET);
				} else {
					printf("%s\n", file.name);
				}
			}
			
			free(file.name);
			free(file.path);
		}
	}
	
	free(paths);
	return ret_value;
}

REGISTER_MODULE(ls);
