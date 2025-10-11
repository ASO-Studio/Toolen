#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

M_ENTRY(elfinfo) {
	if (argc != 2) {
		pplog(P_HELP | P_NAME, "missing operand");
		return 1;
	}

	ElfFileInfo f;
	ElfInfo ei;
	const char *file = argv[1];
	if(openElf(&f, file)) {
		if (errno == 0) {
			pplog(P_NAME, "Not an ELF");
		} else {
			pplog(P_NAME | P_ERRNO, "Failed to open file");
		}
		return 1;
	}

	if (parseElf(&f, &ei)) {
		pplog(P_NAME, "Failed to parse ELF file");
		closeElf(&f);
		return 1;
	}

	printf("%s: ELF", file);
	if (ei.b32) 	printf(" 32-bit");
	else 		printf(" 64-bit");

	printf(" %s", ei.endian == B_ENDIAN ? "MSB" :
				(ei.endian == L_ENDIAN ? "LSB" : "Invalid-Endian"));
	if (ei.pie)	printf(" PIE");
	printf(" %s, %s, version %d (%s),", ei.type, ei.machine, ei.version, ei.abiName);
	if (ei.dynamic) {
		printf(" dynamically linked");
		if (ei.inter) {
			printf(", interrupter %s", ei.inter);
			xfree(ei.inter);
		}
	} else  printf(" statically linked");

	if (ei.debug_info) printf(", with debug_info");

	if (ei.stripped)
		printf(", stripped");
	else	printf(", not stripped");

	printf("\n");
	closeElf(&f);
	return 0;
}

REGISTER_MODULE(elfinfo);
