#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "module.h"
#include "config.h"

static void whoami_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: whoami\n"
		"   --help  show this page\n");
}

int whoami_main(int argc, char *argv[]) {
	if(argv[1] && strcmp(argv[1], "--help") == 0) {
		whoami_show_help();
		return 0;
	}

	struct passwd *pw = NULL;
	pw = getpwuid(getuid());
	if(pw == NULL) {
		perror("whoami");
		return 1;
	}
	printf("%s\n", pw->pw_name);
	return 0;
}

REGISTER_MODULE(whoami);
