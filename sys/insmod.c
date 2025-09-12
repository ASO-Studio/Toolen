/**
 *	insmod.c - Load a kernel module
 *
 * 	Created by RoofAlan
 *		2025/8/11
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static size_t getFdSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) < 0) {
		return 0;
	}

	return st.st_size;
}

static void insmod_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: insmod MODULE\n\n"
			"Load a kernel module\n");
}

M_ENTRY(insmod) {
#if !defined(SYS_init_module)
	#warning SYS_init_module does not support on this platform
	pplog(P_NAME, "does not support on this platform");
	return 1;
#else

	if(!argv[1]) {
		fprintf(stderr, "insmod: Requires 1 argument\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		insmod_show_help();
		return 0;
	}

	isRoot();	// Must run with root

	int fd = open(argv[1], O_RDONLY);
	if(fd < 0) {
		perror("insmod");
		return 1;
	}

	char *params = malloc(1024);	// Default size to allocate
	size_t allocatedSize = 1024;
	size_t sizeCount = 0;

	// Concat all params
	for (int i = 2; argv[i] && params; i++) {
		sizeCount += strlen(argv[i]);
		if(sizeCount > allocatedSize) {
			allocatedSize += strlen(argv[i]) + 1;
			params = realloc(params, allocatedSize);
		}
		strcat(params, argv[i]);
	}

#ifdef SYS_finit_module
	int rc;
	rc = syscall(SYS_finit_module, fd, params, 0);
#else
	int rc = -1;
	errno = ENOSYS;
#endif

	if (rc && (fd == 0 || errno == ENOSYS)) {
		size_t fdsize = getFdSize(fd);
		char *buf = malloc(fdsize);
		if(buf) {
			perror("insmod");
			close(fd);
			free(params);
			return 1;
		}
		read(fd, buf, fdsize);
		rc = syscall(SYS_init_module, buf, fdsize, params);
	}

	if (rc) {
		perror("insmod");
		close(fd);
		free(params);
		return 1;
	}

	free(params);
	close(fd);
	return 0;
#endif
}

REGISTER_MODULE(insmod);
