#include "module.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static void echo_show_help() {
	fprintf(stderr,
		"Toolen "VERSION" "COPYRIGHT"\n\n"
		"echo v1.0  Print string on the display\n"
		"Usage: echo [OPTIONS] [STRING...]\n"
		"Support options:\n"
		"  -n      Print without drawing new line\n"
		"  --help  Show this page\n");
}

int echo_main(int argc, char *argv[]) {
	if(!argv[1]) {
		printf("\n");
		return 0;
	}

	int enter = 1;
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
			default:
				break;
		}
	}

	if (strcmp(argv[1], "--help") == 0) {
		echo_show_help();
		return 0;
	}

	for (int i = count_start; i < argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\b");
	if(enter) printf("\n");
	return 0;
}

REGISTER_MODULE(echo);
