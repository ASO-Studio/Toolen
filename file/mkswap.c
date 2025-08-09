#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/fs.h>

#include "config.h"
#include "module.h"
#include "lib.h"

#define SWAP_SIGNATURE "SWAPSPACE2"
#define SWAP_SIGNATURE_SIZE 10
#define SWAP_LABEL_SIZE 16
#define SWAP_HEADER_SIZE 4096

// Get file size
static off_t fdlength(int fd) {
	off_t len = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	return len;
}

// Generate UUID
static void create_uuid(char *uuid) {
	uuidGen(uuid);
}

// Show UUID
static const char *show_uuid(const char *uuid) {
	return uuid;
}

int mkswap_main(int argc, char *argv[]) {
	int opt;
	char *label = NULL;
	char *device = NULL;
	char buf[4096] = {0};
	
	// Parse command line
	while ((opt = getopt(argc, argv, "L:")) != -1) {
		switch (opt) {
			case 'L': label = optarg; break;
			default:
				fprintf(stderr, "Usage: %s [-L label] device\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	// Get device path
	if (optind >= argc) {
		fprintf(stderr, "%s: missing device argument\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	device = argv[optind];
	
	// Open device
	int fd = open(device, O_RDWR);
	if (fd < 0) {
		perror(device);
		return 0;
	}
	
	// Get page size
	int pagesize = sysconf(_SC_PAGE_SIZE);
	if (pagesize < 0) {
		perror("sysconf");
		exit(EXIT_FAILURE);
	}
	
	// Get file size
	off_t len = fdlength(fd);
	
	// Get numbers of page
	unsigned int pages = (len / pagesize) - 1;
	
	// Set swap header
	unsigned int *swap = (unsigned int *)buf;
	char *swap_label = (char *)(swap + 7);
	char *uuid = (char *)(swap + 3);
	
	// Initialize swap header
	memset(buf, 0, sizeof(buf));
	swap[0] = 1;		// Version number
	swap[1] = pages;	// Last page
	
	// Create UUID
	uuidGen(uuid);

	// Set label
	if (label) {
		strncpy(swap_label, label, SWAP_LABEL_SIZE - 1);
		swap_label[SWAP_LABEL_SIZE - 1] = '\0';
	}
	
	// Write swap header to device
	lseek(fd, 1024, SEEK_SET);
	write(fd, swap, 129 * sizeof(unsigned int));
	
	// Write signature
	lseek(fd, SWAP_HEADER_SIZE - SWAP_SIGNATURE_SIZE, SEEK_SET);
	write(fd, SWAP_SIGNATURE, SWAP_SIGNATURE_SIZE);
	
	// Sync to disk
	fsync(fd);
	close(fd);
	
	// Prepare output
	char label_info[64] = {0};
	if (label) {
		snprintf(label_info, sizeof(label_info), ", LABEL=%.15s", label);
	}

	// Print
	printf("Swapspace size: %luk%s, UUID=%s\n",
		   pages * (unsigned long)(pagesize / 1024),
		   label_info,
		   show_uuid(uuid));
	
	return EXIT_SUCCESS;
}

REGISTER_MODULE(mkswap);
