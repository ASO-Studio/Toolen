#include <stdio.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void uuidgen_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: uuidgen\n\n"
			"Generate an UUID (Version 4)\n");
}

int uuidgen_main(int argc, char *argv[]) {
	if(argc > 1 && findArg(argv, argc, "--help")) {
		uuidgen_show_help();
		return 0;
	}

	char buf[128];
	uuidGen(buf);
	printf("%s\n", buf);
	return 0;
}

REGISTER_MODULE(uuidgen);
