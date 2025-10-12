/**
 *	file.c - Get file type
 *
 * 	Created by RoofAlan
 *		2025/10/12
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

// Display ELF
static int display_elf(const char *file) {
	ElfFileInfo f;
	ElfInfo ei;

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

static int display_file(const char *file) {
	if (access(file, F_OK) != 0) {
		errno = ENOENT;
		pplog(P_NAME | P_ERRNO, "%s", file);
		return 1;
	}

	xioDisableExit();
	if (isElf(file)) {
		xioEnableExit();
		return display_elf(file);
	}

	unsigned char mg[8];
	int fd = xopen2(file, O_RDONLY);

	if (fd < 0)
		return -1;

	// Read magic number
	read(fd, mg, sizeof(mg));
	xclose(fd);

	printf("%s: ", file);

	// Zip
	if (memcmp(mg, "PK""\x03\x04", 4) == 0) {
		printf("Zip compressed data\n");
	} else if (memcmp(mg, "PK""\x05\x06", 4) == 0) {
		printf("Space Zip Archive\n");
	} else if (memcmp(mg, "PK""\x07\x08", 4) == 0) {
		printf("Zip multi-volume archive");
	// GZIP
	} else if (memcmp(mg, "\x1F\x8B", 2) == 0) {
		printf("GZIP compressed data");
		if (mg[2] == '\x08')
			printf(", with deflate\n");
		else	printf("\n");
	// XZ
	} else if (memcmp(mg, "\xFD\x37\x7A\x58\x5A\x00", 6) == 0) {
		printf("XZ compressed data\n");
	// USTAR
	} else if (memcmp(mg, "ustar", 5) == 0) {
		printf("USTAR archive\n");
	// LZMA
	} else if (memcmp(mg, "\x5D\x00\x00", 3) == 0) {
		printf("LZMA compressed data\n");
	// ZSTD
	} else if (memcmp(mg, "\x28\xB5\x2F\xFD", 4) == 0) {
		printf("ZSTD compressed data");
		if (mg[4] == '\0')
			printf("(skip frame)\n");
		else 	printf("\n");
	// BZIP2
	} else if (memcmp(mg, "\x42\x5A\x68", 3) == 0) {
		printf("BZip2 compressed data, block size=%c00K\n", mg[3]);
	// 7-Zip
	} else if (memcmp(mg, "\x37\x7A\xBC\xAF\x27\x1C", 6) == 0) {
		printf("7-Zip compressed data\n");
	// Rar 4.x/5.x
	} else if (memcmp(mg, "\x52\x61\x72\x21\x1A\x07", 6) == 0) {
		printf("RAR");
		if (mg[6] == '\x01' && mg[7] == '\x00')
			printf(" 5.x");
		else 	printf(" 4.x");

		printf(" compressed data\n");
	// LZ4
	} else if (memcmp(mg, "\x04\x22\x4D\x18", 4) == 0) {
		printf("LZ4 compressed data\n");
	// LZH
	} else if (memcmp(mg, "\x2D\x6C\x68", 3) == 0) {
		printf("LZH compressed data\n");
	// Others: data
	} else {
		printf("data\n");
	}

	return 0;
}

static void file_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: file [FILE]...\n\n"
			"Get file type\n");
}

M_ENTRY(file) {
	if (findArg(argv, argc, "--help")) {
		file_show_help();
		return 0;
	}

	if (argc < 2) {
		pplog(P_HELP | P_NAME, "Missing operand");
		return 1;
	}

	int retval = 0;
	for(int i = 1; i < argc; i++) {
		retval = display_file(argv[i]);
	}

	return retval;
}

REGISTER_MODULE(file);
