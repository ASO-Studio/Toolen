/**
 *	md5sum.c - Check SHA1 checksums
 *
 * 	Created by YangZlib
 *	Modified by RoofAlan
 *		2025/8/15
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "module.h"
#include "config.h"
/**
 * @def SHA1_DIGEST_SIZE
 * @brief Size of the SHA1 digest in bytes (160 bits = 20 bytes)
 */
#define SHA1_DIGEST_SIZE 20

/**
 * @def SHA1_BLOCK_SIZE
 * @brief Size of the SHA1 processing block in bytes (512 bits = 64 bytes)
 */
#define SHA1_BLOCK_SIZE 64

/**
 * @struct SHA1_CTX
 * @brief Context structure for SHA1 hashing operations
 * @var SHA1_CTX::state
 * Current hash state (A, B, C, D, E registers)
 * @var SHA1_CTX::bit_len
 * Total length of input data in bits (for padding)
 * @var SHA1_CTX::buffer
 * Buffer to accumulate input data until a full block is formed
 */
typedef struct {
	uint32_t state[5];		// Hash state registers (A-E)
	uint64_t bit_len;		// Total length of input in bits
	uint8_t buffer[SHA1_BLOCK_SIZE];	// Input data buffer
} SHA1_CTX;

// Command-line option flags
static int flag_binary = 0;	// Binary mode (-b/--binary)
static int flag_check = 0;	// Check mode (-c/--check)
static int flag_tag = 0;	// BSD-style output (--tag)
static int flag_text = 1;	// Text mode (-t/--text, default)
static int flag_zero = 0;	// NUL-terminated lines (-z/--zero)
static int flag_ignore_missing = 0;  // Ignore missing files (--ignore-missing)
static int flag_quiet = 0;	// Quiet mode (--quiet)
static int flag_status = 0;	// No output, status code only (--status)
static int flag_strict = 0;	// Strict mode for bad lines (--strict)
static int flag_warn = 0;	// Warn about bad lines (-w/--warn)
static int flag_help = 0;	// Show help (--help)

/**
 * @brief Initial SHA1 state values (defined by FIPS 180-1)
 * These are the magic initial values for the A, B, C, D, E registers
 */
static const uint32_t SHA1_INIT_STATE[5] = {
	0x67452301,	// A
	0xefcdab89,	// B
	0x98badcfe,	// C
	0x10325476,	// D
	0xc3d2e1f0	// E
};

/**
 * @def ROTL32
 * @brief 32-bit left rotation
 * @param x Value to rotate
 * @param n Number of bits to rotate
 * @return Rotated value (unsigned to prevent sign extension issues)
 */
#define ROTL32(x, n) (((uint32_t)(x) << (n)) | ((uint32_t)(x) >> (32 - (n))))

/**
 * @brief Initialize SHA1 context
 * Resets the hash state, buffer, and bit length counter to initial values
 * @param ctx Pointer to SHA1_CTX structure to initialize
 */
static void sha1_init(SHA1_CTX *ctx) {
	// Copy initial state values into context
	memcpy(ctx->state, SHA1_INIT_STATE, sizeof(SHA1_INIT_STATE));
	// Reset total bit length
	ctx->bit_len = 0;
	// Clear input buffer
	memset(ctx->buffer, 0, SHA1_BLOCK_SIZE);
}

/**
 * @brief SHA1 compression function (core transformation)
 * Processes a single 512-bit block of data, updating the hash state
 * Follows the 4-round processing defined in FIPS 180-1
 * @param ctx Pointer to SHA1_CTX containing current state
 * @param block 64-byte (512-bit) block to process
 */
static void sha1_transform(SHA1_CTX *ctx, const uint8_t block[SHA1_BLOCK_SIZE]) {
	uint32_t a = ctx->state[0];	// Register A
	uint32_t b = ctx->state[1];	// Register B
	uint32_t c = ctx->state[2];	// Register C
	uint32_t d = ctx->state[3];	// Register D
	uint32_t e = ctx->state[4];	// Register E
	uint32_t w[80];			// Message schedule array (80 32-bit words)
	int i;

	// Step 1: Initialize W[0..15] from the input block (big-endian conversion)
	for (i = 0; i < 16; i++) {
		w[i] = (block[4*i] << 24) |	// High-order byte
			   (block[4*i + 1] << 16) |
			   (block[4*i + 2] << 8) |
			   block[4*i + 3];	// Low-order byte
	}

	// Step 2: Extend W[16..79] using rotation and XOR
	for (i = 16; i < 80; i++) {
		w[i] = ROTL32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
	}

	// Step 3: 4 rounds of processing with different functions and constants
	// Round 1 (0-19): f = (B & C) | (~B & D), K = 0x5A827999
	for (i = 0; i < 20; i++) {
		uint32_t f = (b & c) | ((~b) & d);
		uint32_t temp = ROTL32(a, 5) + f + e + 0x5A827999 + w[i];
		e = d;
		d = c;
		c = ROTL32(b, 30);
		b = a;
		a = temp;
	}

	// Round 2 (20-39): f = B ^ C ^ D, K = 0x6ED9EBA1
	for (i = 20; i < 40; i++) {
		uint32_t f = b ^ c ^ d;
		uint32_t temp = ROTL32(a, 5) + f + e + 0x6ED9EBA1 + w[i];
		e = d;
		d = c;
		c = ROTL32(b, 30);
		b = a;
		a = temp;
	}

	// Round 3 (40-59): f = (B & C) | (B & D) | (C & D), K = 0x8F1BBCDC
	for (i = 40; i < 60; i++) {
		uint32_t f = (b & c) | (b & d) | (c & d);
		uint32_t temp = ROTL32(a, 5) + f + e + 0x8F1BBCDC + w[i];
		e = d;
		d = c;
		c = ROTL32(b, 30);
		b = a;
		a = temp;
	}

	// Round 4 (60-79): f = B ^ C ^ D, K = 0xCA62C1D6
	for (i = 60; i < 80; i++) {
		uint32_t f = b ^ c ^ d;
		uint32_t temp = ROTL32(a, 5) + f + e + 0xCA62C1D6 + w[i];
		e = d;
		d = c;
		c = ROTL32(b, 30);
		b = a;
		a = temp;
	}

	// Step 4: Update state registers with this block's contribution
	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
}

/**
 * @brief Update SHA1 context with input data
 * Accumulates data in the buffer; processes full blocks when buffer is full
 * @param ctx Pointer to SHA1_CTX structure
 * @param data Input data to hash
 * @param len Length of input data in bytes
 */
static void sha1_update(SHA1_CTX *ctx, const uint8_t *data, size_t len) {
	// Current position in the buffer (0 to SHA1_BLOCK_SIZE-1)
	size_t pos = (ctx->bit_len / 8) % SHA1_BLOCK_SIZE;
	// Update total length (convert bytes to bits)
	ctx->bit_len += (uint64_t)len * 8;
	// Declare i here to ensure visibility in the entire function
	size_t i = 0;

	// Case 1: Input data fills the buffer and possibly more
	if (pos + len >= SHA1_BLOCK_SIZE) {
		// Fill remaining space in buffer and process
		size_t fill = SHA1_BLOCK_SIZE - pos;
		memcpy(ctx->buffer + pos, data, fill);
		sha1_transform(ctx, ctx->buffer);  // Process full block

		// Process remaining full blocks from input data
		for (i = fill; i + SHA1_BLOCK_SIZE <= len; i += SHA1_BLOCK_SIZE) {
			sha1_transform(ctx, data + i);  // Process directly from input
		}

		// Reset position to start of buffer for remaining data
		pos = 0;
	} 
	// Case 2: Input data fits in buffer; no full block to process
	// i remains 0 in this case

	// Save remaining data to buffer
	memcpy(ctx->buffer + pos, data + i, len - i);
}

/**
 * @brief Finalize SHA1 hash computation
 * Adds padding to the remaining data, processes the final block(s),
 * and extracts the digest in big-endian format
 * @param digest Output buffer for the 20-byte SHA1 digest
 * @param ctx Pointer to SHA1_CTX structure
 */
static void sha1_final(uint8_t digest[SHA1_DIGEST_SIZE], SHA1_CTX *ctx) {
	// Current position in the buffer (bytes used)
	size_t pos = (ctx->bit_len / 8) % SHA1_BLOCK_SIZE;

	// Step 1: Add the 0x80 padding marker
	ctx->buffer[pos++] = 0x80;

	// Step 2: If not enough space for 64-bit length, process current block first
	if (pos > SHA1_BLOCK_SIZE - 8) {
		// Fill remaining space with 0s and process
		memset(ctx->buffer + pos, 0, SHA1_BLOCK_SIZE - pos);
		sha1_transform(ctx, ctx->buffer);
		pos = 0;  // Reset for new block
	}

	// Step 3: Fill remaining space with 0s (reserve 8 bytes for length)
	memset(ctx->buffer + pos, 0, SHA1_BLOCK_SIZE - 8 - pos);

	// Step 4: Append total length (64-bit big-endian)
	ctx->buffer[SHA1_BLOCK_SIZE - 8] = (uint8_t)(ctx->bit_len >> 56);
	ctx->buffer[SHA1_BLOCK_SIZE - 7] = (uint8_t)(ctx->bit_len >> 48);
	ctx->buffer[SHA1_BLOCK_SIZE - 6] = (uint8_t)(ctx->bit_len >> 40);
	ctx->buffer[SHA1_BLOCK_SIZE - 5] = (uint8_t)(ctx->bit_len >> 32);
	ctx->buffer[SHA1_BLOCK_SIZE - 4] = (uint8_t)(ctx->bit_len >> 24);
	ctx->buffer[SHA1_BLOCK_SIZE - 3] = (uint8_t)(ctx->bit_len >> 16);
	ctx->buffer[SHA1_BLOCK_SIZE - 2] = (uint8_t)(ctx->bit_len >> 8);
	ctx->buffer[SHA1_BLOCK_SIZE - 1] = (uint8_t)ctx->bit_len;

	// Process the final block
	sha1_transform(ctx, ctx->buffer);

	// Step 5: Convert state registers to digest (big-endian)
	for (int i = 0; i < 5; i++) {
		digest[4*i] = (uint8_t)(ctx->state[i] >> 24);  // High-order byte
		digest[4*i + 1] = (uint8_t)(ctx->state[i] >> 16);
		digest[4*i + 2] = (uint8_t)(ctx->state[i] >> 8);
		digest[4*i + 3] = (uint8_t)ctx->state[i];	   // Low-order byte
	}

	// Clear context for security (avoid leaving sensitive data in memory)
	memset(ctx, 0, sizeof(SHA1_CTX));
}

/**
 * @brief Compute SHA1 hash of a file
 * Opens the file (or reads from stdin), processes all data, and returns the digest
 * @param filename Path to file (use "-" for stdin)
 * @param digest Output buffer for the SHA1 digest
 * @param binary_mode If non-zero, open file in binary mode
 * @return 0 on success, -1 if file cannot be opened, -2 if read error occurs
 */
static int file_sha1(const char *filename, uint8_t digest[SHA1_DIGEST_SIZE], int binary_mode) {
	FILE *file = stdin;  // Default to stdin

	// Open file if not using stdin
	if (filename && strcmp(filename, "-") != 0) {
		// Open in binary or text mode based on flag
		file = fopen(filename, binary_mode ? "rb" : "r");
		if (!file) {
			return -1;  // Failed to open file
		}
	}

	// Initialize hash context
	SHA1_CTX ctx;
	sha1_init(&ctx);

	// Read file in chunks and update hash
	uint8_t buffer[8192];  // 8KB buffer for efficient reading
	size_t bytes_read;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		sha1_update(&ctx, buffer, bytes_read);
	}

	// Check for read errors
	int read_error = ferror(file);

	// Close file (don't close stdin)
	if (file != stdin) {
		fclose(file);
	}

	// Handle read errors
	if (read_error) {
		errno = read_error;  // Preserve error code
		return -2;  // Indicate read error
	}

	// Finalize hash and get digest
	sha1_final(digest, &ctx);
	return 0;  // Success
}

/**
 * @brief Compare two SHA1 digests
 * @param a First digest to compare
 * @param b Second digest to compare
 * @return 1 if digests are equal, 0 otherwise
 */
static int digest_cmp(const uint8_t a[SHA1_DIGEST_SIZE], const uint8_t b[SHA1_DIGEST_SIZE]) {
	return memcmp(a, b, SHA1_DIGEST_SIZE) == 0;
}

/**
 * @brief Convert SHA1 digest to hexadecimal string
 * Produces a lowercase 40-character string (each byte → 2 hex chars)
 * @param digest 20-byte SHA1 digest
 * @param str Output buffer (must be at least 41 bytes)
 */
static void digest2str(const uint8_t digest[SHA1_DIGEST_SIZE], char *str) {
	static const char hex_chars[] = "0123456789abcdef";  // Lowercase hex digits
	for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
		str[2*i] = hex_chars[(digest[i] >> 4) & 0x0F];	// High nibble
		str[2*i + 1] = hex_chars[digest[i] & 0x0F];	// Low nibble
	}
	str[2 * SHA1_DIGEST_SIZE] = '\0';  // Null-terminate
}

/**
 * @brief Convert hexadecimal string to SHA1 digest
 * @param str 40-character hex string
 * @param digest Output buffer for 20-byte digest
 * @return 0 on success, -1 if string is invalid
 */
static int str2digest(const char *str, uint8_t digest[SHA1_DIGEST_SIZE]) {
	// Check string length is correct
	if (strlen(str) != 2 * SHA1_DIGEST_SIZE) {
		return -1;
	}

	// Parse each pair of hex characters
	for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
		unsigned int value;
		// Parse 2-character hex value
		if (sscanf(&str[2*i], "%2x", &value) != 1) {
			return -1;  // Invalid hex character
		}
		digest[i] = (uint8_t)value;
	}

	return 0;  // Success
}

/**
 * @brief Print help message
 * Displays usage information and option descriptions
 */
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: sha1sum [OPTION]... [FILE]...\n");
	printf("Print or check SHA1 (160-bit) checksums.\n\n");
	printf("With no FILE, or when FILE is -, read standard input.\n\n");
	printf("  -b, --binary    read in binary mode\n");
	printf("  -c, --check     read checksums from the FILEs and check them\n");
	printf("      --tag       create a BSD-style checksum\n");
	printf("  -t, --text      read in text mode (default)\n");
	printf("  -z, --zero      end each output line with NUL, not newline,\n");
	printf("                  and disable file name escaping\n\n");
	printf("The following five options are useful only when verifying checksums:\n");
	printf("      --ignore-missing  don't fail or report status for missing files\n");
	printf("      --quiet     don't print OK for each successfully verified file\n");
	printf("      --status    don't output anything, status code shows success\n");
	printf("      --strict    exit non-zero for improperly formatted checksum lines\n");
	printf("  -w, --warn      warn about improperly formatted checksum lines\n\n");
	printf("      --help      display this help and exit\n");
}

/**
 * @brief Parse command-line arguments
 * Uses getopt_long to process options and set corresponding flags
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, -1 on error
 */
static int parse_args(int argc, char *argv[]) {
	// Long options structure (name, has_arg, flag, val)
	static const struct option long_options[] = {
		{"binary",		no_argument, NULL, 'b'},
		{"check",		no_argument, NULL, 'c'},
		{"tag",			no_argument, NULL, 0},
		{"text",		no_argument, NULL, 't'},
		{"zero",		no_argument, NULL, 'z'},
		{"ignore-missing", no_argument, NULL, 0},
		{"quiet",		no_argument, NULL, 0},
		{"status",		no_argument, NULL, 0},
		{"strict",		no_argument, NULL, 0},
		{"warn",		no_argument, NULL, 'w'},
		{"help",		no_argument, NULL, 0},
		{NULL, 0, NULL, 0}  // Terminator
	};

	int opt;
	int option_index = 0;

	// Process options
	while ((opt = getopt_long(argc, argv, "bctzw", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'b':
				flag_binary = 1;
				flag_text = 0;
				break;
			case 'c':
				flag_check = 1;
				break;
			case 't':
				flag_text = 1;
				flag_binary = 0;
				break;
			case 'z':
				flag_zero = 1;
				break;
			case 'w':
				flag_warn = 1;
				break;
			case 0:
				// Handle long options with no short equivalent
				if (strcmp(long_options[option_index].name, "tag") == 0) {
					flag_tag = 1;
				} else if (strcmp(long_options[option_index].name, "ignore-missing") == 0) {
					flag_ignore_missing = 1;
				} else if (strcmp(long_options[option_index].name, "quiet") == 0) {
					flag_quiet = 1;
				} else if (strcmp(long_options[option_index].name, "status") == 0) {
					flag_status = 1;
					flag_quiet = 1;  // --status implies --quiet
				} else if (strcmp(long_options[option_index].name, "strict") == 0) {
					flag_strict = 1;
				} else if (strcmp(long_options[option_index].name, "help") == 0) {
					flag_help = 1;
				} 
				break;
			case '?':
				// getopt_long prints an error message for unrecognized options
				return -1;
			default:
				// Should never reach here
				abort();
		}
	}

	// Check for conflicting options: --binary and --text are mutually exclusive
	if (flag_binary && flag_text) {
		fprintf(stderr, "sha1sum: cannot specify both --binary and --text\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Run in check mode (verify checksums)
 * Reads checksum files, parses lines, computes hashes for referenced files,
 * and verifies they match the expected checksums
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return Exit status (0 for success, non-zero for errors/failures)
 */
static int check_mode(int argc, char *argv[]) {
	int exit_status = 0;  // 0 = success, 1 = failure

	// If no files provided, read from stdin
	if (optind >= argc) {
		static char stdin_flag[] = "-";  // Avoid string literal to char* warning
		argv[argc++] = stdin_flag;
	}

	// Process each checksum file
	for (int i = optind; i < argc; i++) {
		FILE *checkfile = stdin;

		// Open the checksum file (or use stdin)
		if (strcmp(argv[i], "-") != 0) {
			checkfile = fopen(argv[i], "r");
			if (!checkfile) {
				fprintf(stderr, "sha1sum: %s: %s\n", argv[i], strerror(errno));
				exit_status = 1;
				continue;
			}
		}

		char line[1024];  // Buffer to hold lines from checksum file
		int line_num = 0;  // Track line numbers for error messages

		// Read each line from the checksum file
		while (fgets(line, sizeof(line), checkfile)) {
			line_num++;
			char expected_str[41];  // Expected checksum string (40 chars + null)
			char filename[1024];	// Filename from checksum line
			uint8_t expected_digest[SHA1_DIGEST_SIZE];  // Parsed expected digest
			uint8_t computed_digest[SHA1_DIGEST_SIZE];  // Computed digest
			int ret;				// Return value from file_sha1
			int binary_mode = 0;	// Whether file was stored in binary mode
			char mode_char;		 // Mode character (' ' for text, '*' for binary)

			// Process line terminators
			size_t line_len = strlen(line);
			if (line_len > 0 && line[line_len - 1] == '\n') {
				line[line_len - 1] = '\0';  // Remove newline
			} else if (!feof(checkfile) && (flag_warn || flag_strict)) {
				// Line is too long (didn't fit in buffer)
				fprintf(stderr, "sha1sum: %s:%d: improperly formatted checksum line (too long)\n",
						(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
				if (flag_strict) {
					exit_status = 1;
				}
				continue;
			}

			// Parse line based on format (BSD-style or standard)
			if (flag_tag) {
				// BSD-style format: "SHA1 (filename) = checksum"
				if (sscanf(line, "SHA1 (%s) = %40s", filename, expected_str) != 2) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha1sum: %s:%d: improperly formatted BSD-style checksum line\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) {
							exit_status = 1;
						}
					}
					continue;
				}
			} else {
				// Standard format: "checksum mode filename" (mode is ' ' or '*')
				if (sscanf(line, "%40s %c%s", expected_str, &mode_char, filename) != 3) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha1sum: %s:%d: improperly formatted checksum line\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) {
							exit_status = 1;
						}
					}
					continue;
				}
				// Determine if file was stored in binary mode
				binary_mode = (mode_char == '*');
			}

			// Convert expected checksum string to digest
			if (str2digest(expected_str, expected_digest) != 0) {
				if (flag_warn || flag_strict) {
					fprintf(stderr, "sha1sum: %s:%d: invalid checksum value\n",
							(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
					if (flag_strict) {
						exit_status = 1;
					}
				}
				continue;
			}

			// Compute digest for the file (honor binary mode flag)
			ret = file_sha1(filename, computed_digest, binary_mode || flag_binary);
			if (ret == -1) {
				// File not found
				if (!flag_ignore_missing) {
					if (!flag_status) {
						fprintf(stderr, "sha1sum: %s: %s\n", filename, strerror(errno));
					}
					exit_status = 1;
				}
			} else if (ret == -2) {
				// Error reading file
				if (!flag_status) {
					fprintf(stderr, "sha1sum: %s: %s\n", filename, strerror(errno));
				}
				exit_status = 1;
			} else if (!digest_cmp(expected_digest, computed_digest)) {
				// Checksum mismatch
				if (!flag_status) {
					fprintf(stderr, "%s: FAILED\n", filename);
				}
				exit_status = 1;
			} else if (!flag_quiet && !flag_status) {
				// Checksum match
				printf("%s: OK\n", filename);
			}
		}

		// Check for errors reading the checksum file
		if (ferror(checkfile)) {
			fprintf(stderr, "sha1sum: %s: %s\n",
					(checkfile == stdin) ? "(stdin)" : argv[i], strerror(errno));
			exit_status = 1;
		}

		// Close the checksum file (don't close stdin)
		if (checkfile != stdin) {
			fclose(checkfile);
		}
	}

	return exit_status;
}

/**
 * @brief Run in compute mode (generate checksums)
 * Computes SHA1 hashes for specified files (or stdin) and prints them
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return Exit status (0 for success, non-zero for errors)
 */
static int compute_mode(int argc, char *argv[]) {
	int exit_status = 0;
	int use_stdin = (optind >= argc);  // No files specified: read from stdin

	// Process each file
	for (int i = optind; i < argc; i++) {
		uint8_t digest[SHA1_DIGEST_SIZE];
		char digest_str[41];  // 40 hex chars + null terminator
		int ret;

		// Compute hash for the file
		ret = file_sha1(argv[i], digest, flag_binary);
		if (ret != 0) {
			fprintf(stderr, "sha1sum: %s: %s\n", argv[i], strerror(errno));
			exit_status = 1;
			continue;
		}

		// Convert digest to string
		digest2str(digest, digest_str);

		// Print in requested format
		if (flag_tag) {
			// BSD-style: "SHA1 (filename) = checksum"
			printf("SHA1 (%s) = %s", argv[i], digest_str);
		} else {
			// Standard style: "checksum mode filename"
			// Mode is '*' for binary, ' ' for text
			printf("%s %c%s", digest_str, flag_binary ? '*' : ' ', argv[i]);
		}

		// Add line terminator (NUL if --zero, newline otherwise)
		putchar(flag_zero ? '\0' : '\n');
	}

	// Handle stdin if no files were specified
	if (use_stdin) {
		uint8_t digest[SHA1_DIGEST_SIZE];
		char digest_str[41];
		int ret;

		ret = file_sha1("-", digest, flag_binary);
		if (ret != 0) {
			fprintf(stderr, "sha1sum: standard input: %s\n", strerror(errno));
			return 1;
		}

		digest2str(digest, digest_str);

		if (flag_tag) {
			printf("SHA1 (stdin) = %s", digest_str);
		} else {
			printf("%s %c-", digest_str, flag_binary ? '*' : ' ');
		}

		putchar(flag_zero ? '\0' : '\n');
	}

	return exit_status;
}

/**
 * @brief Main function
 * Parses arguments, handles help/version requests, and dispatches to compute/check mode
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit status
 */
M_ENTRY(sha1sum) {
	// Parse command-line arguments
	if (parse_args(argc, argv) != 0) {
		return 1;
	}

	// Show help and exit
	if (flag_help) {
		print_help();
		return 0;
	}

	// Run in check mode or compute mode
	if (flag_check) {
		return check_mode(argc, argv);
	} else {
		return compute_mode(argc, argv);
	}
}
REGISTER_MODULE(sha1sum);
