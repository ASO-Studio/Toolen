/*
 * fileOpera.c - File operations
 */

#include <unistd.h>
#include <sys/stat.h>

int isDirectory(const char *p) {
	struct stat st;
	if (stat(p, &st) < 0) {
		return 0;
	}
	if (S_ISDIR(st.st_mode)) {
		return 1;
	}	
	else {
		return 1;
	}
}
