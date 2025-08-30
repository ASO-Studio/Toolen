#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "config.h"
#include "module.h"

// CRC32 polynomial (IEEE 802.3)
#define CRC32_POLY 0xEDB88320

// Global options
static int check_mode = 0;
static int quiet_mode = 0;
static int status_mode = 0;
static int strict_mode = 0;
static int warn_mode = 0;
static int binary_mode = 0;
static int text_mode = 0;

// CRC32 lookup table
static uint32_t crc32_table[256];

// Initialize CRC32 lookup table
static void init_crc32_table() {
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (int j = 0; j < 8; j++) {
			c = (c & 1) ? (c >> 1) ^ CRC32_POLY : (c >> 1);
		}
		crc32_table[i] = c;
	}
}

// Calculate CRC32 for data buffer
static uint32_t calculate_crc32(const void *data, size_t length) {
	const uint8_t *bytes = (const uint8_t *)data;
	uint32_t crc = 0xFFFFFFFF;
	
	for (size_t i = 0; i < length; i++) {
		crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
	}
	
	return ~crc;
}

// Calculate CRC32 for a file
static int calculate_file_crc32(const char *filename, uint32_t *result) {
	FILE *file;
	uint32_t crc = 0xFFFFFFFF;
	uint8_t buffer[4096];
	size_t bytes_read;
	
	if (strcmp(filename, "-") == 0) {
		file = stdin;
	} else {
		file = fopen(filename, binary_mode ? "rb" : "r");
		if (!file) {
			if (!quiet_mode) {
				fprintf(stderr, "crc32: %s: %s\n", filename, strerror(errno));
			}
			return -1;
		}
	}
	
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		for (size_t i = 0; i < bytes_read; i++) {
			crc = crc32_table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
		}
	}
	
	if (ferror(file)) {
		if (!quiet_mode) {
			fprintf(stderr, "crc32: %s: %s\n", 
					strcmp(filename, "-") == 0 ? "stdin" : filename, 
					strerror(errno));
		}
		if (file != stdin) fclose(file);
		return -1;
	}
	
	if (file != stdin) fclose(file);
	
	*result = ~crc;
	return 0;
}

// Print help information
static void print_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: crc32 [OPTION] [FILE]...\n\n"
		"Print CRC32 checksums for files.\n\n"
		"Suppot options:\n"
		"  -b, --binary      read files in binary mode (default)\n"
		"  -c, --check       read CRC32 sums from files and check them\n"
		"  -t, --text        read files in text mode\n"
		"  -q, --quiet	    suppress all normal output\n"
		"  -s, --status      don't output anything, status code shows success\n"
		"      --strict      exit non-zero for improperly formatted checksum lines\n"
		"  -w, --warn        warn about improperly formatted checksum lines\n\n"
		"With no FILE, or when FILE is -, read standard input.\n");
}

// Process a single file in normal mode
static void process_file(const char *filename) {
	uint32_t crc;
	
	if (calculate_file_crc32(filename, &crc) == 0) {
		if (status_mode) return;
		if (quiet_mode) return;
		
		if (strcmp(filename, "-") == 0) {
			printf("%08x\n", crc);
		} else {
			printf("%08x  %s\n", crc, filename);
		}
	}
}

// Process a file in check mode
static int check_file(const char *filename) {
	FILE *file;
	char line[256];
	int line_number = 0;
	int format_errors = 0;
	int mismatches = 0;
	int total = 0;
	int valid = 0;
	
	if (strcmp(filename, "-") == 0) {
		file = stdin;
	} else {
		file = fopen(filename, "r");
		if (!file) {
			if (!quiet_mode) {
				fprintf(stderr, "crc32: %s: %s\n", filename, strerror(errno));
			}
			return -1;
		}
	}
	
	while (fgets(line, sizeof(line), file)) {
		line_number++;
		total++;
		
		// Parse line: <checksum> [space] [space] <filename>
		uint32_t expected_crc;
		char file_path[256];
		int parsed = 0;
		
		if (sscanf(line, "%08x  %255[^\n]", &expected_crc, file_path) == 2) {
			parsed = 1;
		} else if (sscanf(line, "%08x %255[^\n]", &expected_crc, file_path) == 2) {
			parsed = 1;
		} else if (sscanf(line, "%08x\t%255[^\n]", &expected_crc, file_path) == 2) {
			parsed = 1;
		}
		
		if (!parsed) {
			format_errors++;
			if (warn_mode && !quiet_mode) {
				fprintf(stderr, "crc32: %s: %d: improperly formatted CRC32 line\n", 
						filename, line_number);
			}
			continue;
		}
		
		// Calculate actual CRC
		uint32_t actual_crc;
		if (calculate_file_crc32(file_path, &actual_crc) != 0) {
			mismatches++;
			if (!status_mode && !quiet_mode) {
				printf("%s: FAILED open or read\n", file_path);
			}
			continue;
		}
		
		// Compare results
		if (actual_crc == expected_crc) {
			valid++;
			if (!status_mode && !quiet_mode) {
				printf("%s: OK\n", file_path);
			}
		} else {
			mismatches++;
			if (!status_mode && !quiet_mode) {
				printf("%s: FAILED\n", file_path);
			}
		}
	}
	
	if (ferror(file)) {
		if (!quiet_mode) {
			fprintf(stderr, "crc32: %s: %s\n", 
					strcmp(filename, "-") == 0 ? "stdin" : filename, 
					strerror(errno));
		}
		if (file != stdin) fclose(file);
		return -1;
	}
	
	if (file != stdin) fclose(file);
	
	// Print summary if requested
	if (!status_mode && !quiet_mode && total > 0) {
		printf("\n");
		if (mismatches > 0 || format_errors > 0) {
			printf("%s: FAILED\n", filename);
			printf("%d of %d computed checksums did NOT match", mismatches, total);
			if (format_errors > 0) {
				printf(", %d lines improperly formatted", format_errors);
			}
			printf("\n");
		} else {
			printf("%s: OK\n", filename);
			printf("All %d computed checksums matched\n", total);
		}
	}
	
	// Return status
	if (strict_mode && format_errors > 0) {
		return 1;
	}
	return mismatches > 0 ? 1 : 0;
}

int crc32_main(int argc, char *argv[]) {
	// Initialize CRC table
	init_crc32_table();
	
	// Parse command-line options
	static struct option long_options[] = {
		{"binary",  no_argument, 0, 'b'},
		{"check",   no_argument, 0, 'c'},
		{"text",	no_argument, 0, 't'},
		{"quiet",   no_argument, 0, 'q'},
		{"status",  no_argument, 0, 's'},
		{"strict",  no_argument, 0, 0},
		{"warn",	no_argument, 0, 'w'},
		{"help",	no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	
	int option_index = 0;
	int c;
	
	while ((c = getopt_long(argc, argv, "bctqswhv", long_options, &option_index)) != -1) {
		switch (c) {
			case 'b':
				binary_mode = 1;
				text_mode = 0;
				break;
			case 'c':
				check_mode = 1;
				break;
			case 't':
				text_mode = 1;
				binary_mode = 0;
				break;
			case 'q':
				quiet_mode = 1;
				break;
			case 's':
				status_mode = 1;
				quiet_mode = 1;
				break;
			case 'w':
				warn_mode = 1;
				break;
			case 'h':
				print_help();
				return 0;
			case 0:
				if (strcmp(long_options[option_index].name, "strict") == 0) {
					strict_mode = 1;
				}
				break;
			case '?':
				// Unknown option
				return 2;
			default:
				return 2;
		}
	}
	
	// Handle file arguments
	int exit_status = 0;
	
	if (optind == argc) {
		// No files specified, use stdin
		if (check_mode) {
			exit_status = check_file("-");
		} else {
			process_file("-");
		}
	} else {
		// Process each file
		for (int i = optind; i < argc; i++) {
			if (check_mode) {
				int result = check_file(argv[i]);
				if (result != 0 && exit_status == 0) {
					exit_status = result;
				}
			} else {
				process_file(argv[i]);
			}
		}
	}
	
	return exit_status;
}

REGISTER_MODULE(crc32);
