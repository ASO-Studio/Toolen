/*
 * bdname.c - basename & dirname implementation
 */

#include <string.h>

static void remove_trailing_slashes(char *path) {
	if (path == NULL || *path == '\0') return;
	
	char *end = path + strlen(path) - 1;
	while (end > path && *end == '/') {
		*end-- = '\0';
	}
}

// basename()
char *lib_basename(char *path) {
	// Handle NULL or empty string
	if (path == NULL || *path == '\0') {
		return ".";
	}

	// Remove trailing slashes (preserve root '/')
	remove_trailing_slashes(path);

	// Root directory case ("/" or "///" becomes "/")
	if (*path == '/' && *(path + 1) == '\0') {
		return path;
	}

	// Find last slash and return component after it
	char *last_slash = strrchr(path, '/');
	if (last_slash != NULL) {
		return last_slash + 1;
	}

	// No slashes found - entire path is filename
	return path;
}

// dirname()
char *lib_dirname(char *path) {
	// Handle NULL or empty string
	if (path == NULL || *path == '\0') {
		return ".";
	}

	// Remove trailing slashes (preserve root '/')
	remove_trailing_slashes(path);

	// Root directory case ("/" or "///" becomes "/")
	if (*path == '/' && *(path + 1) == '\0') {
		return path;
	}

	// Find last slash
	char *last_slash = strrchr(path, '/');
	if (last_slash == NULL) {
		return ".";  // No slashes found
	}

	// Handle root case ("/file" -> '/')
	if (last_slash == path) {
		*(last_slash + 1) = '\0';
		return path;
	}

	// Truncate at last slash position
	*last_slash = '\0';
	
	// Remove any new trailing slashes created by truncation
	remove_trailing_slashes(path);
	
	return path;
}
