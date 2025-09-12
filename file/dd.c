/**
 *	dd.c - Copy a file, converting and formatting
 *	       according to the operands
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#define _GNU_SOURCE // For O_DIRECT and O_NOATIME
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "module.h"
#include "lib.h"

#define DEFAULT_BLOCK_SIZE 512

// Define conversion flags
#define CONV_NOTRUNC      (1 << 0)
#define CONV_NOERROR      (1 << 1)
#define CONV_SYNC         (1 << 2)
#define CONV_FSYNC        (1 << 3)
#define CONV_FDATASYNC    (1 << 4)
#define CONV_SPARSE       (1 << 5)

// Define flag options
#define FLAG_DIRECT       (1 << 0)
#define FLAG_DSYNC        (1 << 1)
#define FLAG_SYNC         (1 << 2)
#define FLAG_NONBLOCK     (1 << 3)
#define FLAG_NOATIME      (1 << 4)

// Global variables for status reporting
static off_t total_bytes = 0;
static off_t total_records_in = 0;
static off_t total_records_out = 0;
static struct timeval start_time;

static void dd_show_help(void) {
	SHOW_VERSION(stderr);
	fprintf(stderr,
	"Usage: dd [OPERAND]...\n\n"
	"Copy a file, converting and formatting according to the operands.\n\n"
	"Operands:\n"
	"  bs=BYTES        read and write up to BYTES bytes at a time\n"
	"  cbs=BYTES       convert BYTES bytes at a time\n"
	"  conv=CONVS      convert the file as per the comma separated symbol list\n"
	"  count=N         copy only N input blocks\n"
	"  ibs=BYTES       read up to BYTES bytes at a time (default: 512)\n"
	"  if=FILE         read from FILE instead of stdin\n"
	"  iflag=FLAGS     read as per the comma separated symbol list\n"
	"  obs=BYTES       write BYTES bytes at a time (default: 512)\n"
	"  of=FILE         write to FILE instead of stdout\n"
	"  oflag=FLAGS     write as per the comma separated symbol list\n"
	"  seek=N		  skip N obs-sized blocks at start of output\n"
	"  skip=N		  skip N ibs-sized blocks at start of input\n"
	"  status=LEVEL	The LEVEL of information to print to stderr\n\n"
	"N and BYTES may be followed by multiplicative suffixes:\n"
	"  c=1, w=2, b=512, kB=1000, K=1024, MB=1000 * 1000, M=1024 * 1024,\n"
	"  GB=1000 * 1000 * 1000, G=1024 * 1024 * 1024, and so on for T, P, E, Z, Y.\n\n"
	"Each CONV symbol may be:\n"
	"  ascii, ebcdic, ibm, block, unblock, lcase, ucase, sparse, swab,\n"
	"  sync, excl, nocreat, notrunc, noerror, fdatasync, fsync\n\n"
	"Each FLAG symbol may be:\n"
	"  append, "
#ifdef O_DIRECT
       	"direct,"
#endif
	" directory, dsync, sync, fullblock, nonblock,\n"
	"  noatime, nocache, noctty, nofollow, count_bytes\n");
}

static off_t parse_size(const char *str) {
	char *endptr;
	off_t size = strtoul(str, &endptr, 10);
	
	if (endptr == str) {
		return -1; // No digits found
	}
	
	// Parse multiplicative suffix
	if (*endptr != '\0') {
		if (strcasecmp(endptr, "c") == 0) {
			size *= 1; // character (1 byte)
		} else if (strcasecmp(endptr, "w") == 0) {
			size *= 2; // word (2 bytes)
		} else if (strcasecmp(endptr, "b") == 0) {
			size *= 512; // block (512 bytes)
		} else if (strcasecmp(endptr, "kB") == 0) {
			size *= 1000; // kilobyte (1000 bytes)
		} else if (strcasecmp(endptr, "K") == 0) {
			size *= 1024; // kibibyte (1024 bytes)
		} else if (strcasecmp(endptr, "MB") == 0) {
			size *= 1000000; // megabyte (1000 * 1000 bytes)
		} else if (strcasecmp(endptr, "M") == 0) {
			size *= 1048576; // mebibyte (1024 * 1024 bytes)
		} else if (strcasecmp(endptr, "GB") == 0) {
			size *= 1000000000; // gigabyte (1000 * 1000 * 1000 bytes)
		} else if (strcasecmp(endptr, "G") == 0) {
			size *= 1073741824; // gibibyte (1024 * 1024 * 1024 bytes)
		} else if (strcasecmp(endptr, "TB") == 0 || strcasecmp(endptr, "T") == 0) {
			size *= 1000000000000LL; // terabyte or tebibyte
		} else {
			return -1; // Invalid suffix
		}
	}
	
	return size;
}

static int parse_conv(const char *str, unsigned int *conv_flags) {
	char *copy = xstrdup(str);
	char *token;
	char *rest = copy;
	
	*conv_flags = 0;
	
	while ((token = strtok_r(rest, ",", &rest))) {
		if (strcmp(token, "notrunc") == 0) {
			*conv_flags |= CONV_NOTRUNC;
		} else if (strcmp(token, "noerror") == 0) {
			*conv_flags |= CONV_NOERROR;
		} else if (strcmp(token, "sync") == 0) {
			*conv_flags |= CONV_SYNC;
		} else if (strcmp(token, "fsync") == 0) {
			*conv_flags |= CONV_FSYNC;
		} else if (strcmp(token, "fdatasync") == 0) {
			*conv_flags |= CONV_FDATASYNC;
		} else if (strcmp(token, "sparse") == 0) {
			*conv_flags |= CONV_SPARSE;
		} else {
			xfree(copy);
			return -1; // Unknown conversion
		}
	}
	
	xfree(copy);
	return 0;
}

static int parse_flags(const char *str, unsigned int *flag_flags) {
	char *copy = xstrdup(str);
	char *token;
	char *rest = copy;
	
	*flag_flags = 0;
	
	while ((token = strtok_r(rest, ",", &rest))) {
#ifdef O_DIRECT
		if (strcmp(token, "direct") == 0) {
			*flag_flags |= FLAG_DIRECT;
		} else
#endif
		if (strcmp(token, "dsync") == 0) {
			*flag_flags |= FLAG_DSYNC;
		} else if (strcmp(token, "sync") == 0) {
			*flag_flags |= FLAG_SYNC;
		} else if (strcmp(token, "nonblock") == 0) {
			*flag_flags |= FLAG_NONBLOCK;
		} else if (strcmp(token, "noatime") == 0) {
			*flag_flags |= FLAG_NOATIME;
		} else {
			xfree(copy);
			return -1; // Unknown flag
		}
	}
	
	xfree(copy);
	return 0;
}

static void print_status(int final) {
	struct timeval current_time;
	gettimeofday(&current_time, NULL);
	
	double elapsed = (current_time.tv_sec - start_time.tv_sec) +
					(current_time.tv_usec - start_time.tv_usec) / 1000000.0;
	
	double speed = (elapsed > 0) ? (total_bytes / elapsed) : 0;
	
	char *speed_unit = "B/s";
	double display_speed = speed;
	
	if (display_speed >= 1024) {
		display_speed /= 1024;
		speed_unit = "kB/s";
	}
	if (display_speed >= 1024) {
		display_speed /= 1024;
		speed_unit = "MB/s";
	}
	if (display_speed >= 1024) {
		display_speed /= 1024;
		speed_unit = "GB/s";
	}
	
	fprintf(stderr, "%lld+%lld records in\n", 
			(long long)total_records_in, 0LL);
	fprintf(stderr, "%lld+%lld records out\n", 
			(long long)total_records_out, 0LL);
	fprintf(stderr, "%lld bytes (%s) copied, %.6f s, %.2f %s\n",
			(long long)total_bytes, 
			final ? "final" : "so far",
			elapsed, display_speed, speed_unit);
}

static int is_zero_buffer(const char *buf, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != 0) {
			return 0;
		}
	}
	return 1;
}

M_ENTRY(dd) {
	char *input_file = NULL;
	char *output_file = NULL;
	off_t ibs = DEFAULT_BLOCK_SIZE;	// Input block size
	off_t obs = DEFAULT_BLOCK_SIZE;	// Output block size
	off_t bs = 0;					  // Both input and output block size
	off_t cbs = DEFAULT_BLOCK_SIZE;	// Conversion block size
	off_t count = 0;				   // 0 means copy until EOF
	off_t skip = 0;
	off_t seek = 0;
	unsigned int conv_flags = 0;
	unsigned int iflag_flags = 0;
	unsigned int oflag_flags = 0;
	char *status = "default";
	
	int input_fd = STDIN_FILENO;
	int output_fd = STDOUT_FILENO;
	
	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "if=", 3) == 0) {
			input_file = argv[i] + 3;
		} else if (strncmp(argv[i], "of=", 3) == 0) {
			output_file = argv[i] + 3;
		} else if (strncmp(argv[i], "bs=", 3) == 0) {
			bs = parse_size(argv[i] + 3);
			if (bs <= 0) {
				fprintf(stderr, "dd: invalid block size '%s'\n", argv[i] + 3);
				return 1;
			}
		} else if (strncmp(argv[i], "ibs=", 4) == 0) {
			ibs = parse_size(argv[i] + 4);
			if (ibs <= 0) {
				fprintf(stderr, "dd: invalid input block size '%s'\n", argv[i] + 4);
				return 1;
			}
		} else if (strncmp(argv[i], "obs=", 4) == 0) {
			obs = parse_size(argv[i] + 4);
			if (obs <= 0) {
				fprintf(stderr, "dd: invalid output block size '%s'\n", argv[i] + 4);
				return 1;
			}
		} else if (strncmp(argv[i], "cbs=", 4) == 0) {
			cbs = parse_size(argv[i] + 4);
			if (cbs <= 0) {
				fprintf(stderr, "dd: invalid conversion block size '%s'\n", argv[i] + 4);
				return 1;
			}
		} else if (strncmp(argv[i], "count=", 6) == 0) {
			count = parse_size(argv[i] + 6);
			if (count < 0) {
				fprintf(stderr, "dd: invalid count '%s'\n", argv[i] + 6);
				return 1;
			}
		} else if (strncmp(argv[i], "skip=", 5) == 0) {
			skip = parse_size(argv[i] + 5);
			if (skip < 0) {
				fprintf(stderr, "dd: invalid skip '%s'\n", argv[i] + 5);
				return 1;
			}
		} else if (strncmp(argv[i], "seek=", 5) == 0) {
			seek = parse_size(argv[i] + 5);
			if (seek < 0) {
				fprintf(stderr, "dd: invalid seek '%s'\n", argv[i] + 5);
				return 1;
			}
		} else if (strncmp(argv[i], "conv=", 5) == 0) {
			if (parse_conv(argv[i] + 5, &conv_flags) != 0) {
				fprintf(stderr, "dd: invalid conversion '%s'\n", argv[i] + 5);
				return 1;
			}
		} else if (strncmp(argv[i], "iflag=", 6) == 0) {
			if (parse_flags(argv[i] + 6, &iflag_flags) != 0) {
				fprintf(stderr, "dd: invalid iflag '%s'\n", argv[i] + 6);
				return 1;
			}
		} else if (strncmp(argv[i], "oflag=", 6) == 0) {
			if (parse_flags(argv[i] + 6, &oflag_flags) != 0) {
				fprintf(stderr, "dd: invalid oflag '%s'\n", argv[i] + 6);
				return 1;
			}
		} else if (strncmp(argv[i], "status=", 7) == 0) {
			status = argv[i] + 7;
		} else if (strcmp(argv[i], "--help") == 0) {
			dd_show_help();
			return 0;
		} else {
			fprintf(stderr, "dd: invalid option '%s'\n", argv[i]);
			dd_show_help();
			return 1;
		}
	}

	// If bs is specified, it overrides both ibs and obs
	if (bs > 0) {
		ibs = bs;
		obs = bs;
	}

	// Record start time for statistics
	gettimeofday(&start_time, NULL);

	// Open input file
	int input_flags = O_RDONLY;
#ifdef O_DIRECT
	if (iflag_flags & FLAG_DIRECT) input_flags |= O_DIRECT;
#endif
	if (iflag_flags & FLAG_DSYNC) input_flags |= O_DSYNC;
	if (iflag_flags & FLAG_SYNC) input_flags |= O_SYNC;
	if (iflag_flags & FLAG_NONBLOCK) input_flags |= O_NONBLOCK;
	if (iflag_flags & FLAG_NOATIME) input_flags |= O_NOATIME;
	
	if (input_file) {
		input_fd = xopen2(input_file, input_flags);
	}
	
	// Open output file
	int output_flags = O_WRONLY | O_CREAT;
	if (!(conv_flags & CONV_NOTRUNC)) {
		output_flags |= O_TRUNC;
	}
#ifdef O_DIRECT
	if (oflag_flags & FLAG_DIRECT) output_flags |= O_DIRECT;
#endif
	if (oflag_flags & FLAG_DSYNC) output_flags |= O_DSYNC;
	if (oflag_flags & FLAG_SYNC) output_flags |= O_SYNC;
	if (oflag_flags & FLAG_NONBLOCK) output_flags |= O_NONBLOCK;
	if (oflag_flags & FLAG_NOATIME) output_flags |= O_NOATIME;
	
	if (output_file) {
		output_fd = xopen(output_file, output_flags, 0644);
	}
	
	// Skip input blocks if requested
	if (skip > 0) {
		if (lseek(input_fd, skip * ibs, SEEK_SET) < 0) {
			fprintf(stderr, "dd: failed to skip %ld blocks: %s\n", skip, strerror(errno));
			if (input_file) xclose(input_fd);
			if (output_file) xclose(output_fd);
			return 1;
		}
	}
	
	// Seek output blocks if requested
	if (seek > 0) {
		if (lseek(output_fd, seek * obs, SEEK_SET) < 0) {
			fprintf(stderr, "dd: failed to seek %ld blocks: %s\n", seek, strerror(errno));
			if (input_file) xclose(input_fd);
			if (output_file) xclose(output_fd);
			return 1;
		}
	}
	
	// Allocate buffers for data transfer
	char *inbuf = xmalloc(ibs);
	char *outbuf = xmalloc(obs);
	// Copy data
	off_t blocks_copied = 0;
	ssize_t bytes_read;
	
	while ((count == 0 || blocks_copied < count)) {
		bytes_read = read(input_fd, inbuf, ibs);
		
		if (bytes_read < 0) {
			if (conv_flags & CONV_NOERROR) {
				fprintf(stderr, "dd: read error: %s (continuing)\n", strerror(errno));
				continue;
			} else {
				fprintf(stderr, "dd: read error: %s\n", strerror(errno));
				xfree(inbuf);
				xfree(outbuf);
				if (input_file) xclose(input_fd);
				if (output_file) xclose(output_fd);
				return 1;
			}
		}

		if (bytes_read == 0) {
			break; // EOF
		}

		// Handle sync conversion (pad with zeros)
		if ((conv_flags & CONV_SYNC) && bytes_read < ibs) {
			memset(inbuf + bytes_read, 0, ibs - bytes_read);
			bytes_read = ibs;
		}

		total_records_in++;
		total_bytes += bytes_read;

		// Handle sparse conversion
		if ((conv_flags & CONV_SPARSE) && is_zero_buffer(inbuf, bytes_read)) {
			if (lseek(output_fd, bytes_read, SEEK_CUR) < 0) {
				fprintf(stderr, "dd: seek error: %s\n", strerror(errno));
				xfree(inbuf);
				xfree(outbuf);
				if (input_file) xclose(input_fd);
				if (output_file) xclose(output_fd);
				return 1;
			}
			total_records_out++;
			blocks_copied++;
			continue;
		}

		// Write data (handle different input and output block sizes)
		ssize_t bytes_to_write = bytes_read;
		ssize_t bytes_written = 0;
		
		while (bytes_to_write > 0) {
			ssize_t chunk_size = (bytes_to_write > obs) ? obs : bytes_to_write;
			memcpy(outbuf, inbuf + bytes_written, chunk_size);

			ssize_t written = write(output_fd, outbuf, chunk_size);
			if (written < 0) {
				fprintf(stderr, "dd: write error: %s\n", strerror(errno));
				xfree(inbuf);
				xfree(outbuf);
				if (input_file) xclose(input_fd);
				if (output_file) xclose(output_fd);
				return 1;
			}
			
			bytes_to_write -= written;
			bytes_written += written;
		}
		
		total_records_out++;
		blocks_copied++;
		
		// Print progress if requested
		if (strcmp(status, "progress") == 0) {
			print_status(0);
		}
	}
	
	// Sync data if requested
	if (conv_flags & CONV_FSYNC) {
		if (fsync(output_fd) < 0) {
			fprintf(stderr, "dd: fsync error: %s\n", strerror(errno));
		}
	} else if (conv_flags & CONV_FDATASYNC) {
		if (fdatasync(output_fd) < 0) {
			fprintf(stderr, "dd: fdatasync error: %s\n", strerror(errno));
		}
	}
	
	// Print final statistics unless suppressed
	if (strcmp(status, "none") != 0 && strcmp(status, "noxfer") != 0) {
		print_status(1);
	}
	
	// Cleanup
	xfree(inbuf);
	xfree(outbuf);
	if (input_file) xclose(input_fd);
	if (output_file) xclose(output_fd);
	
	return 0;
}

REGISTER_MODULE(dd);
