#include <stdio.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>
#include <getopt.h>

#include "config.h"
#include "module.h"

static void uname_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: uname [-asnrvmo]\n\n"
			"Print system information\n\n"
			"Support options:\n"
			"  -a  Print all supported informaton\n"
			"  -s  Print system name\n"
			"  -n  Print network(domain) name\n"
			"  -r  Kernel Release number\n"
			"  -v  Kernel Version\n"
			"  -m  Machine (hardware) name\n"
			"  -o  Userspace type\n");
}

int uname_main(int argc, char *argv[]) {

	int opt;
	int opt_index = 0;

	struct {
		bool sys;	// System name
		bool node;	// Network name
		bool release;	// Kernel Release
		bool version;	// Kernel Version
		bool machine;	// Hardware name
		bool userspace;	// Userspace type
	} toShow;
	toShow.sys = false;
	toShow.node = false;
	toShow.release = false;
	toShow.version = false;
	toShow.machine = false;
	toShow.userspace = false;

	static struct option opts[] = {
		{"a", no_argument, 0, 'a'},
		{"s", no_argument, 0, 's'},
		{"n", no_argument, 0, 'n'},
		{"r", no_argument, 0, 'r'},
		{"v", no_argument, 0, 'v'},
		{"m", no_argument, 0, 'm'},
		{"o", no_argument, 0, 'o'},
		{"help", no_argument, 0, 'h'}
	};

	if (argc < 2) {
		toShow.sys = true;	// Default: Print system name
	}

	while ((opt = getopt_long(argc, argv, "asnrvmoh", opts, &opt_index)) != -1 ) {
		switch(opt) {
			case 'h':
				uname_show_help();
				return 0;
			case 'a': {
					toShow.sys = true;
					toShow.node = true;
					toShow.release = true;
					toShow.version = true;
					toShow.machine = true;
					toShow.userspace = true;
					break;
				}
			case 's': toShow.sys = true; break;
			case 'n': toShow.node = true; break;
			case 'r': toShow.release = true; break;
			case 'v': toShow.version = true; break;
			case 'm': toShow.machine = true; break;
			case 'o': toShow.userspace = true; break;
			case '?':
				  fprintf(stderr, "Try pass '--help' for more details\n");
				  return 1;
			default:
				  abort();
		}
	}

	struct utsname uts;
	if(uname(&uts) < 0) {
		perror("uname");
		return 1;
	}

	if(toShow.sys)		// Print system name
		printf("%s ", uts.sysname);
	if(toShow.node)		// Print node name
		printf("%s ", uts.nodename);
	if(toShow.release)	// Print Kernel Release
		printf("%s ", uts.release);
	if(toShow.version)	// Print Kernel Version
		printf("%s ", uts.version);
	if(toShow.machine)	// Print machine
		printf("%s ", uts.machine);
	// TODO: Print userspace type
	if(toShow.userspace)
		printf("GNU/Linux ");	// Print GNU/Linux by default

	printf("\b \b\n");

	return 0;
}

REGISTER_MODULE(uname);
