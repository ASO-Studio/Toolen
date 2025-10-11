#include <stdio.h>
#include <string.h>

#if __has_include(<sys/io.h>)
# include <sys/io.h>
#endif

#include "config.h"
#include "module.h"
#include "lib.h"
#include "debug.h"

static void rport_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: rport <read|write> <...>\n\n"
			"Read/Write a byte from/to PORT(s)\n\n"
			"Format:\n"
			" Read:\n"
			"   Port1 Port2 Port3 ...\n"
			" Write:\n"
			"   Port1 Value1 Port2 Value2 ...\n");
}

M_ENTRY(rport) {
#if defined(__x86_64__) || defined(__i386__)
	if (findArg(argv, argc, "--help")) {
		rport_show_help();
		return 0;
	}

	if (argc < 3) {
		pplog(P_NAME | P_HELP, "missing arguments");
		return 1;
	}

	int mode = 0; // 0->read 1->write
	if (strcasecmp(argv[1], "read") == 0) {
		mode = 0;
	} else if (strcasecmp(argv[1], "write") == 0) {
		mode = 1;
	} else {
		pplog(P_NAME | P_HELP, "Unknown mode: %s\n", argv[1]);
	}

	if (iopl(3)) {
		perror("iopl");
		return 1;
	}

	for (int i = 2; i < argc; i++) {
		if (argv[i][0] == '-') continue;

		unsigned long port = strtold(argv[i], NULL);
		if (mode) {
			unsigned short value = strtold(argv[++i] ? : "0", NULL);
			LOG("Send '0x%02x' to port 0x%02lx...\n", value, port);
			outb(port, value);
		} else {
			printf("0x%02x\n", inb(port));
		}
	}
	return 0;
#else
	(void)argc; (void)argv;
	fprintf(stderr, "This command only supports the x86(_64) architecture\n");
	return 1;
#endif
}

REGISTER_MODULE(rport);
