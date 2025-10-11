/**
 *	elfinfo.c - Get ELF informations
 *
 * 	Created by RoofAlan
 *		2025/10/11
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void elfinfo_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: elfinfo [FILE]...\n\n"
			"Get ELF informations\n");
}

// Display one file
static int display_file(const char *file) {
	ElfFileInfo f;
	ElfInfo ei;

	// Check if 'file' pointed to an ELF
	if (!isElf(file)) {
		pplog(P_NAME, "%s: Not an ELF", file);
		return 1;
	}

	// Open
	if(openElf(&f, file)) {
		pplog(P_NAME | P_ERRNO, "%s", file);
		return 1;
	}

	// Parse
	if (parseElf(&f, &ei)) {
		pplog(P_NAME, "Failed to parse ELF file");
		closeElf(&f);
		return 1;
	}

	// Display
	printf("%s: ELF", file);

	// Bit
	if (ei.b32) 	printf(" 32-bit");
	else 		printf(" 64-bit");

	// Endian
	printf(" %s", ei.endian == B_ENDIAN ? "MSB" :
				(ei.endian == L_ENDIAN ? "LSB" : "Invalid-Endian"));

	// PIE
	if (ei.pie)	printf(" PIE");

	// type, machine, version and OS ABI Name
	printf(" %s, %s, version %d (%s),", ei.type, ei.machine, ei.version, ei.abiName);

	// Link type
	if (ei.dynamic) {
		printf(" dynamically linked");
		if (ei.inter) {
			printf(", interpreter %s", ei.inter);
			xfree(ei.inter);
		}
	} else  printf(" statically linked");

	// Debug info
	if (ei.debug_info) printf(", with debug_info");

	// Strip
	if (ei.stripped)
		printf(", stripped");
	else	printf(", not stripped");

	printf("\n");
	closeElf(&f);
	return 0;
}

M_ENTRY(elfinfo) {
	if(findArg(argv, argc, "--help")) {
		elfinfo_show_help();
		return 0;
	}

	if (argc < 2) {
		pplog(P_HELP | P_NAME, "missing operand");
		return 1;
	}

	for(int i = 1; i < argc; i++) {
		display_file(argv[i]);
	}

	return 0;
}

REGISTER_MODULE(elfinfo);
