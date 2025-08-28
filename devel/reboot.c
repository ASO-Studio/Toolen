/**
 *	reboot.c - Restart system
 *
 * 	Created by RoofAlan
 *		2025/8/14
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void reboot_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: reboot [Options]\n\n"
			"Restart system\n\n"
			"Support options:\n"
			"  -d  Wait before restarting, like -d 1.5m(90s)\n"
			"  -f  Force restarting\n"
			"  -n  Don't sync system works\n"
			"Tips: If you're using systemd as a init program, we don't recommand you use this.\n");
}

int reboot_main(int argc, char *argv[]) {
	int opt;
	int opt_index = 0;
	static struct option opts[] = {
		{"d", required_argument, 0, 'd'},
		{"f", no_argument, 0, 'f'},
		{"n", no_argument, 0, 'n'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	int delay = 0;
	unsigned long delaySecs = 0;
	int force = 0;
	int syncRes = 1;

	while((opt = getopt_long(argc, argv, "d:fh", opts, &opt_index) ) != -1) {
		switch(opt) {
			case 'd': {
					delay = 1;
					delaySecs = timeToSeconds(argv[optind-1]);
					break;
				}
			case 'f': force = 1; break;
			case 'n': syncRes = 0; break;
			case 'h': reboot_show_help(); return 0;
			case '?': fprintf(stderr, "Try pass '--help' for more details\n"); return 1;
			default:
				abort();
		}
	}

	// Check permission
	isRoot();

	int cmd = RB_AUTOBOOT;
#if 0
	fprintf(stderr, "Sending SIGTERM to all processes\n");
	kill(-1, SIGTERM);
	fprintf(stderr, "Sending SIGKILL to all processes\n");
	kill(-1, SIGINT);
#endif	// TODO: Send SIGTERM & SIGKILL to all processes
	if (force) {
		// TODO: Force reboot
	}

	if (delay) {
		usleep(delaySecs * 1000000);
	}

	if(syncRes) {
		sync();
	}
	if (reboot(cmd) < 0) {
		perror("reboot");
		return 1;
	}
	return 0;
}

REGISTER_MODULE(reboot);
