/*
 * main.c - Main program
 */
#include "module.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define _IS(x) (strcmp(argv[1], x) == 0)

#if defined(COMPILE_LICENSE)
__attribute__((used)) const char LICENSE[] =
"\n\nMIT License\n\n"
"Copyright (c) 2025 RoofAlan\n\n"
"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
"of this software and associated documentation files (the \"Software\"), to deal\n"
"in the Software without restriction, including without limitation the rights\n"
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
"copies of the Software, and to permit persons to whom the Software is\n"
"furnished to do so, subject to the following conditions:\n\n"
"The above copyright notice and this permission notice shall be included in all\n"
"copies or substantial portions of the Software.\n\n"
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
"SOFTWARE.\n\n";
#endif // COMPILE_LICENSE

// Show help
static void show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: toolen [command|options] [Args...]\n");
	fprintf(stderr,
		"Support options: \n"
		"  --help, -h      Show this page\n"
		"  --list, -l      List all support commands\n"
		"  --version, -v   Show version\n\n");
	fprintf(stderr, "Support commands: \n");
	list_all_modules();
}

static const char *basename(const char *path) {
	const char *p = strrchr(path, '/');
	if(p) return p+1;
	return path;
}

int main(int argc, char *argv[]) {
	const char *progname = basename(argv[0]); // Would you like './program'?

	if (find_module(progname) != 0) {	// Try to find program from argv[0], this is to solve symlinks
		if(argc <= 1) {
			show_help();
			return 1;
		} else if (find_module(argv[1]) == 0 && argv[1][0] != '-') { // It doesn't start with '-' and it's an exist module
			return run_module(argv[1], argc - 1, &argv[1]);
		} else if (argv[1][0] == '-') {
			if(argv[1][1] == '-') { // "--", like --help, --list, e.g
				if(_IS("--help")) {
					show_help();
					return 0;
				} else if (_IS("--list")) {
					list_all_modules();
					return 0;
				} else if (_IS("--version")) {
					printf(PROGRAM_NAME" "VERSION"\n");
					return 0;
				} else {
					fprintf(stderr, "Unknown option: %s\n", argv[1]);
					return 1;
				}
			} else {	// "-", like -h, -l, e.g.
				switch(argv[1][1]) {
					case 'h':	// -h
						show_help();
						return 0;
					case 'l':	// -l
						list_all_modules();
						return 0;
					case 'v':	// -v
						printf(PROGRAM_NAME" "VERSION"\n");
						return 0;
					default:
						fprintf(stderr,"Unknown options: -- '%c'\n", argv[1][1]);
						return 1;
				}
			}
		} else {	// Not an option and cannot find a suitable command
			fprintf(stderr, "%s: %s: Command not found\n", progname, argv[1]);
			return 1;
		}
	} else {	// Found module from argv[0], then run it directly
		return run_module(progname, argc, argv);
	}

	return 0;
}
