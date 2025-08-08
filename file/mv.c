#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>
#include <ctype.h>
#include <stdarg.h>

#include "module.h"
#include "config.h"

#define DEFAULT_BACKUP_SUFFIX "~"

// Backup strategy type
typedef enum {
	BACKUP_NONE,	 // No backup
	BACKUP_SIMPLE,   // Simple backup (e.g., filename~)
	BACKUP_NUMBERED  // Numbered backup (e.g., filename~1~)
} BackupType;

// Update strategy type
typedef enum {
	UPDATE_ALL,		// Update all existing files
	UPDATE_NONE,		// Do not update any existing files
	UPDATE_NONE_FAIL,	// Fail if trying to update existing files
	UPDATE_OLDER		// Update only if source is newer than destination
} UpdateType;

// Global option variables
static struct {
	int force;		// -f: force overwrite
	int interactive;	// -i: interactive prompt before overwrite
	int no_clobber;		// -n: do not overwrite existing files
	int verbose;		// -v: show detailed operation information
	int debug;		// --debug: show debug information
	int exchange;		// --exchange: swap source and destination
	int no_copy;		// --no-copy: do not copy if rename fails
	int strip_trailing;	// --strip-trailing-slashes: remove trailing slashes from source paths
	int no_target_dir;	// -T: do not treat destination as directory
	char *target_dir;	// -t: target directory for multiple sources
	const char *suffix;	// -S: custom backup suffix
	BackupType backup_type;	// Type of backup to create
	UpdateType update_type;	// Strategy for updating existing files
} opts = {
	.suffix = DEFAULT_BACKUP_SUFFIX,
	.backup_type = BACKUP_NONE,
	.update_type = UPDATE_ALL
};

// Error handling with formatted message
static void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

// Verbose output (shown if -v or --debug is enabled)
static void verbose(const char *fmt, ...) {
	if (opts.verbose || opts.debug) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
		putchar('\n');
	}
}

// Debug output (shown only if --debug is enabled)
static void debug(const char *fmt, ...) {
	if (opts.debug) {
		fputs("debug: ", stdout);
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
		putchar('\n');
	}
}

// Remove trailing slashes from path if enabled
static char *strip_slashes(char *path) {
	if (!opts.strip_trailing || !path) return path;
	size_t len = strlen(path);
	while (len > 1 && path[len-1] == '/') {
		path[--len] = '\0';
	}
	return path;
}

// Check if path points to a directory
static int is_dir(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Check if destination file/directory exists
static int dest_exists(const char *dest) {
	return access(dest, F_OK) == 0;
}

// Generate backup filename based on backup type
static char *get_backup_name(const char *dest) {
	static char buf[1024];
	if (opts.backup_type == BACKUP_NONE) return NULL;

	if (opts.backup_type == BACKUP_SIMPLE) {
		snprintf(buf, sizeof(buf), "%s%s", dest, opts.suffix);
		return buf;
	}

	int max_num = 0;
	char pattern[1024], tmp[1024];
	snprintf(pattern, sizeof(pattern), "%s~%%d~", dest);
	// Find highest existing numbered backup
	for (int i = 1; i <= 1000; i++) { 
		snprintf(tmp, sizeof(tmp), pattern, i);
		if (access(tmp, F_OK) == 0) max_num = i;
		else break;
	}
	snprintf(buf, sizeof(buf), pattern, max_num + 1);
	return buf;
}

// Create backup of destination before overwriting
static void create_backup(const char *dest) {
	if (opts.backup_type == BACKUP_NONE || !dest_exists(dest)) return;
	char *backup = get_backup_name(dest);
	if (!backup) return;
	debug("creating backup: %s -> %s", dest, backup);
	if (rename(dest, backup) != 0) die("failed to create backup '%s'", backup);
	verbose("backed up '%s' to '%s'", dest, backup);
}

// Prompt user before overwriting (interactive mode)
static int prompt_override(const char *dest) {
	fprintf(stderr, "mv: overwrite '%s'? ", dest);
	fflush(stderr);
	char c;
	if (read(STDIN_FILENO, &c, 1) != 1) return 0;
	return tolower(c) == 'y';
}

// Determine if update is needed based on update strategy
static int need_update(const char *src, const char *dest) {
	if (!dest_exists(dest)) return 1; 

	switch (opts.update_type) {
		case UPDATE_ALL: return 1;
		case UPDATE_NONE: return 0;
		case UPDATE_NONE_FAIL: die("mv: cannot overwrite '%s' (--update=none-fail)", dest);
		case UPDATE_OLDER: {
			struct stat src_st, dest_st;
			if (stat(src, &src_st) != 0 || stat(dest, &dest_st) != 0)
				die("mv: failed to stat files for update check");
			return src_st.st_mtime > dest_st.st_mtime; 
		}
		default: return 1;
	}
}

// Copy content from source file to destination file
static void copy_file(const char *src, const char *dest) {
	int fd_src = open(src, O_RDONLY);
	if (fd_src < 0) die("cannot open '%s' for reading", src);
	int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd_dest < 0) { close(fd_src); die("cannot open '%s' for writing", dest); }

	char buf[4096];
	ssize_t n;
	while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
		if (write(fd_dest, buf, n) != n) {
			close(fd_src); close(fd_dest);
			die("failed to write to '%s'", dest);
		}
	}
	if (n < 0) die("failed to read from '%s'", src);
	close(fd_src);
	close(fd_dest);
}

// Recursively copy directory contents
static void copy_dir(const char *src, const char *dest) {
	mkdir(dest, 0755);
	DIR *dir = opendir(src);
	if (!dir) die("cannot open directory '%s'", src);

	struct dirent *entry;
	char src_path[1024], dest_path[1024];
	while ((entry = readdir(dir))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
		snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

		if (is_dir(src_path)) copy_dir(src_path, dest_path);
		else copy_file(src_path, dest_path);
	}
	closedir(dir);
}

// Recursively delete directory and its contents
static void remove_dir(const char *path) {
	DIR *dir = opendir(path);
	if (!dir) die("cannot open directory '%s'", path);

	struct dirent *entry;
	char child[1024];
	while ((entry = readdir(dir))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
		if (is_dir(child)) remove_dir(child);
		else if (unlink(child) != 0) die("cannot remove '%s'", child);
	}
	closedir(dir);
	if (rmdir(path) != 0) die("cannot remove directory '%s'", path);
}

// Handle moving a single source to destination
static void move_single(const char *src, const char *dest) {
	debug("processing: %s -> %s", src, dest);

	int dest_is_dir = !opts.no_target_dir && is_dir(dest);
	char final_dest[1024];
	if (dest_is_dir) {
		const char *base = basename((char*)src);
		snprintf(final_dest, sizeof(final_dest), "%s/%s", dest, base);
	} else {
		strncpy(final_dest, dest, sizeof(final_dest)-1);
	}

	if (!need_update(src, final_dest)) {
		verbose("skipped '%s' (no update needed)", src);
		return;
	}

	if (dest_exists(final_dest) && opts.interactive && !prompt_override(final_dest)) {
		verbose("skipped '%s' (user declined)", src);
		return;
	}

	create_backup(final_dest);

	// Try atomic rename first
	if (rename(src, final_dest) == 0) {
		verbose("renamed '%s' -> '%s'", src, final_dest);
		return;
	}

	// Handle non-cross-filesystem errors
	if (errno != EXDEV) {
		if (errno == EEXIST && !opts.force && !opts.no_clobber)
			die("mv: cannot overwrite '%s' without -f", final_dest);
		die("mv: failed to rename '%s' to '%s'", src, final_dest);
	}

	// Cross-filesystem move: copy then delete (unless --no-copy)
	if (opts.no_copy) die("mv: cannot move across filesystems (--no-copy)");
	debug("copying across filesystems: %s -> %s", src, final_dest);

	if (is_dir(src)) {
		copy_dir(src, final_dest);
		remove_dir(src);
	} else {
		copy_file(src, final_dest);
		if (unlink(src) != 0) die("cannot remove source '%s'", src);
	}
	verbose("moved '%s' -> '%s'", src, final_dest);
}

// Print help information
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: mv [OPTION]... [-T] SOURCE DEST\n");
	printf("  or:  mv [OPTION]... SOURCE... DIRECTORY\n");
	printf("  or:  mv [OPTION]... -t DIRECTORY SOURCE...\n\n");
	printf("Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n\n");
	printf("Mandatory arguments to long options are mandatory for short options too.\n");
	printf("  --backup[=CONTROL]       make a backup of each existing destination file\n");
	printf("  -b                       like --backup but does not accept an argument\n");
	printf("      --debug              explain how a file is copied.  Implies -v\n");
	printf("      --exchange           exchange source and destination\n");
	printf("  -f, --force              do not prompt before overwriting\n");
	printf("  -i, --interactive        prompt before overwrite\n");
	printf("  -n, --no-clobber         do not overwrite an existing file\n"
		"\n");
	printf("If you specify more than one of -i, -f, -n, only the final one takes effect.\n");
	printf("        --no-copy          do not copy if renaming fails\n");
	printf("        --strip-trailing-slashes  remove any trailing slashes from each SOURCE\n");
	printf("                                  argument\n");
	printf("  -S, --suffix=SUFFIX             override the usual backup suffix\n");
	printf("  -t, --target-directory=DIRECTORY  move all SOURCE arguments into DIRECTORY\n");
	printf("  -T, --no-target-directory       treat DEST as a normal file\n");
	printf("	  --update[=UPDATE]       control which existing files are updated;\n");
	printf("                                  UPDATE={all,none,none-fail,older(default)}.\n");
	printf("  -u                              equivalent to --update[=older].  See below\n");
	printf("  -v, --verbose                   explain what is being done\n");
	printf("      --help                      display this help and exit\n");
	exit(EXIT_SUCCESS);
}

// Parse backup control string
static void parse_backup_control(const char *control) {
	if (!control || strcmp(control, "none") == 0 || strcmp(control, "off") == 0)
		opts.backup_type = BACKUP_NONE;
	else if (strcmp(control, "simple") == 0 || strcmp(control, "never") == 0)
		opts.backup_type = BACKUP_SIMPLE;
	else if (strcmp(control, "numbered") == 0 || strcmp(control, "t") == 0)
		opts.backup_type = BACKUP_NUMBERED;
	else die("mv: invalid backup control '%s'", control);
}

// Parse update strategy string
static void parse_update_type(const char *update) {
	if (!update || strcmp(update, "older") == 0)
		opts.update_type = UPDATE_OLDER;
	else if (strcmp(update, "all") == 0)
		opts.update_type = UPDATE_ALL;
	else if (strcmp(update, "none") == 0)
		opts.update_type = UPDATE_NONE;
	else if (strcmp(update, "none-fail") == 0)
		opts.update_type = UPDATE_NONE_FAIL;
	else die("mv: invalid update type '%s'", update);
}

// Main logic for mv command
int mv_main(int argc, char *argv[]) {
	static const struct option long_opts[] = {
		{"backup", optional_argument, NULL, 'B'},
		{"debug", no_argument, NULL, 'D'},
		{"exchange", no_argument, NULL, 'X'},
		{"force", no_argument, NULL, 'f'},
		{"interactive", no_argument, NULL, 'i'},
		{"no-clobber", no_argument, NULL, 'n'},
		{"no-copy", no_argument, NULL, 'C'},
		{"strip-trailing-slashes", no_argument, NULL, 's'},
		{"suffix", required_argument, NULL, 'S'},
		{"target-directory", required_argument, NULL, 't'},
		{"no-target-directory", no_argument, NULL, 'T'},
		{"update", optional_argument, NULL, 'U'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int c;
	while ((c = getopt_long(argc, argv, "bfint:TS:uv", long_opts, NULL)) != -1) {
		switch (c) {
			case 'b': parse_backup_control("simple"); break;
			case 'B': parse_backup_control(optarg); break;
			case 'D': opts.debug = 1; opts.verbose = 1; break;
			case 'X': opts.exchange = 1; break;
			case 'f': opts.force = 1; opts.interactive = 0; opts.no_clobber = 0; break;
			case 'i': opts.interactive = 1; opts.force = 0; opts.no_clobber = 0; break;
			case 'n': opts.no_clobber = 1; opts.force = 0; opts.interactive = 0; break;
			case 'C': opts.no_copy = 1; break;
			case 's': opts.strip_trailing = 1; break;
			case 'S': opts.suffix = optarg; break;
			case 't': opts.target_dir = optarg; break;
			case 'T': opts.no_target_dir = 1; break;
			case 'U': parse_update_type(optarg); break;
			case 'u': parse_update_type("older"); break;
			case 'v': opts.verbose = 1; break;
			case 'h': print_help(); break;
			default: die("Try '%s --help' for more information.", argv[0]);
		}
	}

	char **sources = &argv[optind];
	int num_sources = argc - optind;
	char *dest = NULL;

	if (opts.target_dir) {
		if (num_sources == 0) die("mv: missing source arguments");
		dest = opts.target_dir;
		if (!is_dir(dest)) die("mv: target '%s' is not a directory", dest);
	} else {
		if (num_sources < 1) die("mv: missing operand");
		if (num_sources == 1) die("mv: missing destination");
		dest = sources[num_sources - 1];
		num_sources--;
		if (num_sources > 1 && !opts.no_target_dir && !is_dir(dest))
			die("mv: target '%s' is not a directory", dest);
	}

	if (opts.exchange) {
		if (num_sources != 1) die("mv: --exchange requires exactly one source");
		char *tmp = sources[0];
		sources[0] = dest;
		dest = tmp;
	}

	for (int i = 0; i < num_sources; i++) {
		char *src = strip_slashes(sources[i]);
		if (!src || access(src, F_OK) != 0) die("mv: cannot stat '%s': No such file", src);
		move_single(src, dest);
	}
	return 0;
}

REGISTER_MODULE(mv);
