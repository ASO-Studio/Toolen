#include "module.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Parse escapes
static int parse_escape(const char *s, int *consumed) {
	int c = *(++s);
	*consumed = 2;

	switch (c) {
		case 'a': return '\a';
		case 'b': return '\b';
		case 'f': return '\f';
		case 'n': return '\n';
		case 'r': return '\r';
		case 't': return '\t';
		case 'v': return '\v';
		case '\\': return '\\';
		case '\'': return '\'';
		case '\"': return '\"';
		case '?': return '\?';
		case 'e': return 27;
		case 'c': return -1; 
		case 'x': {
			int val = 0, count = 0;
			s++; *consumed = 2;
			while (isxdigit(*s) && count++ < 2) {
				val = val * 16 + (isdigit(*s) ? *s - '0' : tolower(*s) - 'a' + 10);
				s++; (*consumed)++;
			}
			return val;
		}
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7': {
			int val = c - '0', count = 1;
			const char *p = s + 1;
			while (*p >= '0' && *p <= '7' && count < 3) {
				val = val * 8 + (*p - '0');
				p++; count++;
			}
			*consumed = 1 + count;
			return val;
		}
		default:
			return c;
	}
}

// Print string with parsing escapes
static int ansi_echo(const char *s) {
	while (*s) {
		if (*s == '\\') {
			int consumed = 0;
			int c = parse_escape(s, &consumed);
			if (c == -1) return 1; 
			putchar(c);
			s += consumed;
		} else {
			putchar(*s++);
		}
	}
	return 0;
}

// Show help page
static void echo_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr,
		"echo v1.0  Print string on the display\n"
		"Usage: echo [OPTIONS] [STRING...]\n"
		"Support options:\n"
		"  -e      Print string with interpretation of backslash escapes\n"
		"  -n      Print without drawing new line\n"
		"  --help  Show this page\n");
}

int echo_main(int argc, char *argv[]) {
	if(!argv[1]) {
		printf("\n");
		return 0;
	}

	int enter = 1;
	int ansi = 0;
	int count_start = 1;

	if(argv[1][0] == '-' && argv[1][1] != 0) {
		switch(argv[1][1]) {
			case 'n':
				enter = 0;
				count_start++;
				break;
			case 'h':
				echo_show_help();
				return 0;
			case 'e':
				ansi = 1;
				count_start++;
				break;
			case 'v':
				JUST_VERSION();
				return 0;
			default:
				break;
		}
	}

	if (strcmp(argv[1], "--help") == 0) {
		echo_show_help();
		return 0;
	}

	for (int i = count_start; i < argc; i++) {
		if (ansi) {	// ANSI Enabled
			ansi_echo(argv[i]);
			printf(" ");
		} else {	// ANSI Disabled
			printf("%s ", argv[i]);
		}
	}
	printf("\b");
	if(enter)	// If user didn't pass '-n' and then draw a new line
		printf("\n");
	return 0;
}

REGISTER_MODULE(echo);
