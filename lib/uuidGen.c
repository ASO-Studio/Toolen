/*
 * uuidGen.c - Generate UUID (Version 4)
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

void uuidGen(char *uuid) {
	unsigned char bytes[16];
	int fd;

	// Open /dev/urandom
	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		if (read(fd, bytes, sizeof(bytes)) != sizeof(bytes)) {
			// Failed to read, then use fallback
			close(fd);
			fd = -1;
		} else {
			close(fd);
		}
	}

	// If /dev/urandom is invalid, try fallback
	if (fd < 0) {
		srand(time(NULL) ^ getpid());
		for (int i = 0; i < sizeof(bytes); i++) {
			bytes[i] = rand() & 0xFF;
		}
	}

	bytes[6] = (bytes[6] & 0x0F) | 0x40; // Version 4
	bytes[8] = (bytes[8] & 0x3F) | 0x80;

	// Format UUID string
	snprintf(uuid, 37, "%02x%02x%02x%02x-"
			"%02x%02x-"
			"%02x%02x-"
			"%02x%02x-"
			"%02x%02x%02x%02x%02x%02x",
			bytes[0], bytes[1], bytes[2], bytes[3],
			bytes[4], bytes[5],
			bytes[6], bytes[7],
			bytes[8], bytes[9],
			bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}
