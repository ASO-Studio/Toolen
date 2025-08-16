#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <fcntl.h>

#include "config.h"
#include "module.h"

static bool markWithD = false;
static bool tabAsI = false;

// Show help informations
static int cat_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: cat [options] [FILE]...\n\n"
			"Copy (concatenate) files to stdout. If no files gave, copy from stdin\n\n"
			"Support options:\n"
			"  -e   Mark each newline with $\n"
			"  -t   Show tabs as ^I\n");
}

static void catFd(int fd) {
	lseek(fd, 0, SEEK_SET);
	char ch[2];
	int readRet = 0;
	while(1) {
		if((readRet = read(fd, ch, 1)) <= 0) {
			break;
		}
		if(markWithD && ch[0] == '\n') {
			write(1, "$", 1);
		}
		if(tabAsI && ch[0] == '\t') {
			write(1, "^I", 2);
			continue;
		}
		write(1, ch, 1);
	}
}

// Main function
int cat_main(int argc, char *argv[]) {
	int opt;
	int optidx = 0;
	struct option opts[] = {
		{"e", no_argument, 0, 'e'},
		{"t", no_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "ethv", opts, &optidx)) != -1) {
		switch(opt) {
			case 'e':
				markWithD = true;
				break;
			case 't':
				tabAsI = true;
				break;
			case 'h':
				cat_show_help();
				return 0;
			case 'v':
				JUST_VERSION();
				return 0;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
			default:
				abort();
		}
	}

	if(argc - optind <= 0) {
		catFd(0);
	}

	int ret = 0;
	for(int i=1; argv[i]; i++) {
		if(argv[i][0] != '-') {
			int fd = open(argv[i], O_RDONLY);
			if(fd < 0) {
				perror(argv[i]);
				ret = 1;
				continue;
			}
			catFd(fd);
			close(fd);
		}
	}

	return ret;
}

REGISTER_MODULE(cat);
