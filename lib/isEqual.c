/*
 * isEqual.c - Check if a string is an equation
 */

#include <string.h>

// return: 0->false, 1->true
int isEquation(const char *str) {
	// NULL
	if (!str) return 0;

	// =xxx
	if (*str == '=') return 0;

	// xxx
	if (!strstr(str, "=")) return 0;

	// xxx=xxx / xxx=
	return 1;
}
