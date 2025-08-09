/*
 * argsFind.c - Find some thing arguments
 */

#include <string.h>

int findArg(char *args[], int argc, const char *str) {
	for(int i = 0; i < argc; i++) {
		if(strcmp(args[i], str) == 0) {
			return 1;
		}
	}
	return 0;
}
