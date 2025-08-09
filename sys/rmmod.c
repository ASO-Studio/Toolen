#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/syscall.h>

#include "config.h"
#include "module.h"

static void rmmod_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: rmmod [options] ModuleName ...\n\n"
			"Unload kernel modules\n\n"
			"Support options:\n"
			"  -f, --force Force unload module\n"
			"  -w, --wait  Wait until module is no longer used\n"
			"  -h, --help  Show this page\n");
}

int rmmod_main(int argc, char *argv[]) {

	if(argc < 2) {
		fprintf(stderr, "Usage: rmmod [options] ModuleName ...\n");
		fprintf(stderr, "Try pass '--help' for more details\n");
		return 1;
	}

	int opt;
	int option_index = 0;
	static struct option opts[] = {
		{"help", no_argument, 0, 'h'},
		{"force", no_argument, 0, 'f'},
		{"wait", no_argument, 0, 'w'},
		{NULL, 0, 0, 0},
	};

	int shouldWait = 0;
	int shouldForce = 0;

	while ((opt = getopt_long(argc, argv, "hwf", opts, &option_index)) != -1) {
		switch(opt) {
			case 'h':
				rmmod_show_help();
				return 0;
			case 'w':
				shouldWait = 1;
				break;
			case 'f':
				shouldForce = 1;
				break;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
			default:
				abort();
		}
	}

	if (optind > argc || optind < 0 || !argv[optind]) {
		fprintf(stderr, "rmmod: Name required\n");
		return 1;
	}

	int flags = 0;

	if (shouldWait) {
		flags |= O_NONBLOCK;
	}
	if (shouldForce) {
		flags |= O_TRUNC;
	}

	int ret;
	for(int i = optind; i <= argc - optind; i++) {
		ret = syscall(SYS_delete_module, argv[i], flags);
		if (ret) {
			perror("rmmod");
			return 1;
		}
	}

	return 0;
}

REGISTER_MODULE(rmmod);
