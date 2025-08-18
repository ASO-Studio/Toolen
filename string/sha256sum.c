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
 * @def SHA256_DIGEST_SIZE
 * @brief Size of SHA-256 digest in bytes (256 bits = 32 bytes)
 */
#define SHA256_DIGEST_SIZE 32

/**
 * @def SHA256_BLOCK_SIZE
 * @brief Size of SHA-256 processing block in bytes (512 bits = 64 bytes)
 */
#define SHA256_BLOCK_SIZE 64

/**
 * @def SHA256_ROUNDS
 * @brief Number of transformation rounds in SHA-256
 */
#define SHA256_ROUNDS 64

/**
 * @struct SHA256_CTX
 * @brief Context structure for SHA-256 hashing operations
 * @var SHA256_CTX::state
 * Current hash state (8 x 32-bit registers: a-h)
 * @var SHA256_CTX::bit_len
 * Total length of input data in bits (for padding)
 * @var SHA256_CTX::buffer
 * Buffer to accumulate input data until a full 512-bit block is formed
 */
typedef struct {
	uint32_t state[8];		// Hash state registers
	uint64_t bit_len;		// Total input length in bits
	uint8_t buffer[SHA256_BLOCK_SIZE];	// Input data buffer
} SHA256_CTX;

// Command-line option flags
static int flag_binary = 0;	// Binary mode (-b/--binary)
static int flag_check = 0;	// Check mode (-c/--check)
static int flag_tag = 0;	// BSD-style output (--tag)
static int flag_text = 1;	// Text mode (-t/--text, default)
static int flag_zero = 0;	// NUL-terminated lines (-z/--zero)
static int flag_ignore_missing = 0;	// Ignore missing files (--ignore-missing)
static int flag_quiet = 0;	// Quiet mode (--quiet)
static int flag_status = 0;	// No output, status code only (--status)
static int flag_strict = 0;	// Strict mode for bad lines (--strict)
static int flag_warn = 0;	// Warn about bad lines (-w/--warn)
static int flag_help = 0;	// Show help (--help)

/**
 * @brief SHA-256 initial hash values (FIPS 180-4 specification)
 * Derived from the first 32 bits of the fractional parts of the square roots
 * of the first 8 prime numbers (2, 3, 5, 7, 11, 13, 17, 19).
 */
static const uint32_t SHA256_INIT_STATE[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/**
 * @brief SHA-256 round constants (K[0..63])
 * Derived from the first 32 bits of the fractional parts of the cube roots
 * of the first 64 prime numbers.
 */
static const uint32_t SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/**
 * @brief Right rotation macro
 * @param x 32-bit value to rotate
 * @param n Number of bits to rotate (0 < n < 32)
 * @return Rotated value
 */
#define ROTR(x, n) ((x >> n) | (x << (32 - n)))

/**
 * @brief SHA-256 Sigma0 function (for state)
 * @param x 32-bit input word
 * @return Transformed value
 */
#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))

/**
 * @brief SHA-256 Sigma1 function (for state)
 * @param x 32-bit input word
 * @return Transformed value
 */
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))

/**
 * @brief SHA-256 Gamma0 function (for message schedule)
 * @param x 32-bit input word
 * @return Transformed value
 */
#define GAMMA0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3))

/**
 * @brief SHA-256 Gamma1 function (for message schedule)
 * @param x 32-bit input word
 * @return Transformed value
 */
#define GAMMA1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10))

/**
 * @brief Choice function: selects bits from y or z based on x
 * @param x, y, z 32-bit input words
 * @return (x & y) ^ (~x & z)
 */
#define CH(x, y, z) ((x & y) ^ (~x & z))

/**
 * @brief Majority function: selects the most common bit among x, y, z
 * @param x, y, z 32-bit input words
 * @return (x & y) ^ (x & z) ^ (y & z)
 */
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

/**
 * @brief Initialize SHA-256 context
 * Resets the hash state to initial values, clears the input buffer,
 * and resets the total bit length counter.
 * @param ctx Pointer to SHA256_CTX structure to initialize
 */
static void sha256_init(SHA256_CTX *ctx) {
	// Copy initial state values
	memcpy(ctx->state, SHA256_INIT_STATE, sizeof(SHA256_INIT_STATE));
	// Reset total input length (in bits)
	ctx->bit_len = 0;
	// Clear the input buffer
	memset(ctx->buffer, 0, SHA256_BLOCK_SIZE);
}

/**
 * @brief SHA-256 compression function (core transformation)
 * Processes a single 512-bit (64-byte) block of data and updates the hash state.
 * Implements the 64-round transformation as specified in FIPS 180-4.
 * @param ctx Pointer to SHA256_CTX containing current state
 * @param block 64-byte (512-bit) input block to process
 */
static void sha256_transform(SHA256_CTX *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
	uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
	uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
	uint32_t w[64];  // Message schedule array (64 32-bit words)
	uint32_t t1, t2; // Temporary values for each round
	int i;

	// Step 1: Initialize message schedule W[0..15] from input block
	// Convert 4-byte chunks of the block to 32-bit big-endian words
	for (i = 0; i < 16; i++) {
		w[i] = (block[4*i] << 24) |	// High-order byte
			   (block[4*i + 1] << 16) |
			   (block[4*i + 2] << 8) |
			   block[4*i + 3];		 // Low-order byte
	}

	// Step 2: Extend W[16..63] using the Gamma functions
	for (i = 16; i < 64; i++) {
		w[i] = GAMMA1(w[i-2]) + w[i-7] + GAMMA0(w[i-15]) + w[i-16];
	}

	// Step 3: 64 rounds of processing
	for (i = 0; i < 64; i++) {
		t1 = h + SIGMA1(e) + CH(e, f, g) + SHA256_K[i] + w[i];
		t2 = SIGMA0(a) + MAJ(a, b, c);
		
		// Update working variables for next round
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	// Step 4: Add the transformed values back to the original state
	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
}

/**
 * @brief Update SHA-256 context with input data
 * Accumulates input data in the buffer. When the buffer is full (64 bytes),
 * processes the block using sha256_transform and resets the buffer.
 * @param ctx Pointer to SHA256_CTX structure
 * @param data Pointer to input data to hash
 * @param len Length of input data in bytes
 */
static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len) {
	// Current position in the buffer (0 to SHA256_BLOCK_SIZE-1)
	size_t pos = (ctx->bit_len / 8) % SHA256_BLOCK_SIZE;
	// Update total length of input data (convert bytes to bits)
	ctx->bit_len += (uint64_t)len * 8;
	// Declare 'i' here to ensure scope covers entire function (fix for compiler error)
	size_t i = 0;

	// Case 1: Input data fills the buffer and possibly more
	if (pos + len >= SHA256_BLOCK_SIZE) {
		// Fill remaining space in the buffer and process the full block
		size_t fill = SHA256_BLOCK_SIZE - pos;
		memcpy(ctx->buffer + pos, data, fill);
		sha256_transform(ctx, ctx->buffer);

		// Process remaining full blocks directly from input data
		for (i = fill; i + SHA256_BLOCK_SIZE <= len; i += SHA256_BLOCK_SIZE) {
			sha256_transform(ctx, data + i);
		}

		// Reset buffer position for remaining data
		pos = 0;
	}
	// Case 2: Input data fits in buffer (no full block to process) - 'i' remains 0

	// Save remaining data to buffer
	memcpy(ctx->buffer + pos, data + i, len - i);
}

/**
 * @brief Finalize SHA-256 hash computation
 * Adds padding to the remaining data in the buffer (as specified by FIPS 180-4),
 * processes the final block(s), and extracts the 256-bit digest.
 * @param digest Output buffer for the 32-byte SHA-256 digest
 * @param ctx Pointer to SHA256_CTX structure
 */
static void sha256_final(uint8_t digest[SHA256_DIGEST_SIZE], SHA256_CTX *ctx) {
	// Current position in the buffer (bytes used)
	size_t pos = (ctx->bit_len / 8) % SHA256_BLOCK_SIZE;

	// Step 1: Add the 0x80 padding marker (required by SHA-256 standard)
	ctx->buffer[pos++] = 0x80;

	// Step 2: If there's not enough space for the 64-bit length, process current block
	if (pos > SHA256_BLOCK_SIZE - 8) {
		// Fill remaining space with zeros and process the block
		memset(ctx->buffer + pos, 0, SHA256_BLOCK_SIZE - pos);
		sha256_transform(ctx, ctx->buffer);
		// Reset position for a new block
		pos = 0;
	}

	// Step 3: Fill remaining space with zeros (reserve 8 bytes for length)
	memset(ctx->buffer + pos, 0, SHA256_BLOCK_SIZE - 8 - pos);

	// Step 4: Append total length of input data (64-bit big-endian)
	ctx->buffer[SHA256_BLOCK_SIZE - 8] = (uint8_t)(ctx->bit_len >> 56);
	ctx->buffer[SHA256_BLOCK_SIZE - 7] = (uint8_t)(ctx->bit_len >> 48);
	ctx->buffer[SHA256_BLOCK_SIZE - 6] = (uint8_t)(ctx->bit_len >> 40);
	ctx->buffer[SHA256_BLOCK_SIZE - 5] = (uint8_t)(ctx->bit_len >> 32);
	ctx->buffer[SHA256_BLOCK_SIZE - 4] = (uint8_t)(ctx->bit_len >> 24);
	ctx->buffer[SHA256_BLOCK_SIZE - 3] = (uint8_t)(ctx->bit_len >> 16);
	ctx->buffer[SHA256_BLOCK_SIZE - 2] = (uint8_t)(ctx->bit_len >> 8);
	ctx->buffer[SHA256_BLOCK_SIZE - 1] = (uint8_t)ctx->bit_len;

	// Process the final block
	sha256_transform(ctx, ctx->buffer);

	// Step 5: Extract the 256-bit digest from the state (big-endian)
	for (int i = 0; i < 8; i++) {
		digest[4*i] = (uint8_t)(ctx->state[i] >> 24);  // High-order byte
		digest[4*i + 1] = (uint8_t)(ctx->state[i] >> 16);
		digest[4*i + 2] = (uint8_t)(ctx->state[i] >> 8);
		digest[4*i + 3] = (uint8_t)ctx->state[i];	   // Low-order byte
	}

	// Clear context for security (prevent sensitive data leakage)
	memset(ctx, 0, sizeof(SHA256_CTX));
}

/**
 * @brief Compute SHA-256 hash of a file
 * Opens a file (or reads from standard input), processes all data,
 * and returns the SHA-256 digest.
 * @param filename Path to the file (use "-" for standard input)
 * @param digest Output buffer for the 32-byte digest
 * @param binary_mode If non-zero, open file in binary mode
 * @return 0 on success, -1 if file cannot be opened, -2 if read error occurs
 */
static int file_sha256(const char *filename, uint8_t digest[SHA256_DIGEST_SIZE], int binary_mode) {
	FILE *file = stdin;  // Default to standard input

	// Open the file if not using standard input
	if (filename && strcmp(filename, "-") != 0) {
		// Open in binary or text mode based on the flag
		file = fopen(filename, binary_mode ? "rb" : "r");
		if (!file) {
			return -1;  // Failed to open file
		}
	}

	// Initialize SHA-256 context
	SHA256_CTX ctx;
	sha256_init(&ctx);

	// Read file in chunks and update hash (8KB buffer for efficiency)
	uint8_t buffer[8192];
	size_t bytes_read;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		sha256_update(&ctx, buffer, bytes_read);
	}

	// Check for errors during reading
	int read_error = ferror(file);

	// Close the file (do not close standard input)
	if (file != stdin) {
		fclose(file);
	}

	// Handle read errors
	if (read_error) {
		errno = read_error;  // Preserve the error code
		return -2;  // Indicate read error
	}

	// Finalize the hash and get the digest
	sha256_final(digest, &ctx);
	return 0;  // Success
}

/**
 * @brief Compare two SHA-256 digests
 * @param a First digest to compare
 * @param b Second digest to compare
 * @return 1 if digests are equal, 0 otherwise
 */
static int digest_cmp(const uint8_t a[SHA256_DIGEST_SIZE], const uint8_t b[SHA256_DIGEST_SIZE]) {
	return memcmp(a, b, SHA256_DIGEST_SIZE) == 0;
}

/**
 * @brief Convert SHA-256 digest to hexadecimal string
 * Converts the 32-byte digest to a 64-character lowercase hex string.
 * @param digest 32-byte SHA-256 digest
 * @param str Output buffer (must be at least 65 bytes to hold null terminator)
 */
static void digest2str(const uint8_t digest[SHA256_DIGEST_SIZE], char *str) {
	static const char hex_chars[] = "0123456789abcdef";  // Lowercase hex digits
	for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
		str[2*i] = hex_chars[(digest[i] >> 4) & 0x0F];  // High nibble (4 bits)
		str[2*i + 1] = hex_chars[digest[i] & 0x0F];	 // Low nibble (4 bits)
	}
	str[2 * SHA256_DIGEST_SIZE] = '\0';  // Null-terminate the string
}

/**
 * @brief Convert hexadecimal string to SHA-256 digest
 * Parses a 64-character hex string into a 32-byte digest.
 * @param str 64-character hexadecimal string
 * @param digest Output buffer for the 32-byte digest
 * @return 0 on success, -1 if string is invalid (wrong length or bad characters)
 */
static int str2digest(const char *str, uint8_t digest[SHA256_DIGEST_SIZE]) {
	// Check if the string length is correct (64 characters for 32 bytes)
	if (strlen(str) != 2 * SHA256_DIGEST_SIZE) {
		return -1;
	}

	// Parse each pair of hex characters into a byte
	for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
		unsigned int val;
		// Read 2-character hex value
		if (sscanf(&str[2*i], "%2x", &val) != 1) {
			return -1;  // Invalid hex character
		}
		digest[i] = (uint8_t)val;
	}

	return 0;  // Success
}

/**
 * @brief Print help message
 * Displays usage information and describes all supported options.
 */
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: sha256sum [OPTION]... [FILE]...\n");
	printf("Print or check SHA256 (256-bit) checksums.\n\n");
	printf("With no FILE, or when FILE is -, read standard input.\n\n");
	printf("  -b, --binary          read in binary mode\n");
	printf("  -c, --check           read checksums from the FILEs and check them\n");
	printf("      --tag             create a BSD-style checksum\n");
	printf("  -t, --text            read in text mode (default)\n");
	printf("  -z, --zero            end each output line with NUL, not newline,\n");
	printf("                          and disable file name escaping\n\n");
	printf("The following five options are useful only when verifying checksums:\n");
	printf("      --ignore-missing  don't fail or report status for missing files\n");
	printf("      --quiet           don't print OK for each successfully verified file\n");
	printf("      --status          don't output anything, status code shows success\n");
	printf("      --strict          exit non-zero for improperly formatted checksum lines\n");
	printf("  -w, --warn            warn about improperly formatted checksum lines\n\n");
	printf("      --help            display this help and exit\n");
}

/**
 * @brief Parse command-line arguments
 * Uses getopt_long to process options and set corresponding flags.
 * Validates for conflicting options (e.g., --binary and --text).
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on success, -1 on error (invalid options or conflicts)
 */
static int parse_args(int argc, char *argv[]) {
	// Long options structure: (name, has_arg, flag, val)
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

	int opt;				// Current option
	int option_index = 0;   // Index for long options

	// Process all options
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
		fprintf(stderr, "sha256sum: cannot specify both --binary and --text\n");
		return -1;
	}

	return 0;  // Success
}

/**
 * @brief Run in check mode (verify checksums)
 * Reads checksum files, parses each line, computes hashes for referenced files,
 * and verifies they match the expected checksums.
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit status (0 for success, non-zero for errors or checksum mismatches)
 */
static int check_mode(int argc, char *argv[]) {
	int exit_status = 0;  // 0 = success, 1 = failure

	// If no files are provided, read checksums from standard input
	if (optind >= argc) {
		static char stdin_flag[] = "-";  // Avoids string literal to char* warning
		argv[argc++] = stdin_flag;
	}

	// Process each checksum file
	for (int i = optind; i < argc; i++) {
		FILE *checkfile = stdin;  // Default to standard input

		// Open the checksum file if it's not standard input
		if (strcmp(argv[i], "-") != 0) {
			checkfile = fopen(argv[i], "r");
			if (!checkfile) {
				fprintf(stderr, "sha256sum: %s: %s\n", argv[i], strerror(errno));
				exit_status = 1;
				continue;
			}
		}

		char line[1024];  // Buffer to hold lines from the checksum file
		int line_num = 0;  // Track line numbers for error messages

		// Read each line from the checksum file
		while (fgets(line, sizeof(line), checkfile)) {
			line_num++;
			char expected_str[65];  // Expected checksum string (64 chars + null)
			char filename[1024];	// Filename from the checksum line
			uint8_t expected_digest[SHA256_DIGEST_SIZE];  // Parsed expected digest
			uint8_t computed_digest[SHA256_DIGEST_SIZE];  // Computed digest
			int ret;				// Return value from file_sha256
			int binary_mode = 0;	// Whether the file was stored in binary mode
			char mode_char;		 // Mode character (' ' for text, '*' for binary)

			// Process line terminators
			size_t line_len = strlen(line);
			if (line_len > 0 && line[line_len - 1] == '\n') {
				line[line_len - 1] = '\0';  // Remove newline character
			} else if (!feof(checkfile) && (flag_warn || flag_strict)) {
				// Line is too long (didn't fit in the buffer)
				fprintf(stderr, "sha256sum: %s:%d: improperly formatted checksum line (too long)\n",
						(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
				if (flag_strict) {
					exit_status = 1;
				}
				continue;
			}

			// Parse the line based on the selected format (BSD-style or standard)
			if (flag_tag) {
				// BSD-style format: "SHA256 (filename) = checksum"
				if (sscanf(line, "SHA256 (%s) = %64s", filename, expected_str) != 2) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha256sum: %s:%d: improperly formatted BSD-style checksum line\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) {
							exit_status = 1;
						}
					}
					continue;
				}
			} else {
				// Standard format: "checksum mode filename" (mode is ' ' or '*')
				if (sscanf(line, "%64s %c%s", expected_str, &mode_char, filename) != 3) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha256sum: %s:%d: improperly formatted checksum line\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) {
							exit_status = 1;
						}
					}
					continue;
				}
				// Determine if the file was stored in binary mode
				binary_mode = (mode_char == '*');
			}

			// Convert the expected checksum string to a digest
			if (str2digest(expected_str, expected_digest) != 0) {
				if (flag_warn || flag_strict) {
					fprintf(stderr, "sha256sum: %s:%d: invalid checksum value\n",
							(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
					if (flag_strict) {
						exit_status = 1;
					}
				}
				continue;
			}

			// Compute the digest for the file (honor binary mode)
			ret = file_sha256(filename, computed_digest, binary_mode || flag_binary);
			if (ret == -1) {
				// File not found
				if (!flag_ignore_missing) {
					if (!flag_status) {
						fprintf(stderr, "sha256sum: %s: %s\n", filename, strerror(errno));
					}
					exit_status = 1;
				}
			} else if (ret == -2) {
				// Error reading the file
				if (!flag_status) {
					fprintf(stderr, "sha256sum: %s: %s\n", filename, strerror(errno));
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
			fprintf(stderr, "sha256sum: %s: %s\n",
					(checkfile == stdin) ? "(stdin)" : argv[i], strerror(errno));
			exit_status = 1;
		}

		// Close the checksum file (do not close standard input)
		if (checkfile != stdin) {
			fclose(checkfile);
		}
	}

	return exit_status;
}

/**
 * @brief Run in compute mode (generate checksums)
 * Computes SHA-256 hashes for the specified files (or standard input)
 * and prints them in the selected format.
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit status (0 for success, non-zero for errors)
 */
static int compute_mode(int argc, char *argv[]) {
	int exit_status = 0;
	int use_stdin = (optind >= argc);  // No files specified: read from stdin

	// Process each file
	for (int i = optind; i < argc; i++) {
		uint8_t digest[SHA256_DIGEST_SIZE];
		char digest_str[65];  // 64 hex chars + null terminator
		int ret;

		// Compute the SHA-256 hash for the file
		ret = file_sha256(argv[i], digest, flag_binary);
		if (ret != 0) {
			fprintf(stderr, "sha256sum: %s: %s\n", argv[i], strerror(errno));
			exit_status = 1;
			continue;
		}

		// Convert the digest to a hex string
		digest2str(digest, digest_str);

		// Print the checksum in the selected format
		if (flag_tag) {
			// BSD-style format: "SHA256 (filename) = checksum"
			printf("SHA256 (%s) = %s", argv[i], digest_str);
		} else {
			// Standard format: "checksum mode filename"
			// Mode is '*' for binary, ' ' for text
			printf("%s %c%s", digest_str, flag_binary ? '*' : ' ', argv[i]);
		}

		// Add line terminator (NUL if --zero, newline otherwise)
		putchar(flag_zero ? '\0' : '\n');
	}

	// Handle standard input if no files were specified
	if (use_stdin) {
		uint8_t digest[SHA256_DIGEST_SIZE];
		char digest_str[65];
		int ret;

		ret = file_sha256("-", digest, flag_binary);
		if (ret != 0) {
			fprintf(stderr, "sha256sum: standard input: %s\n", strerror(errno));
			return 1;
		}

		digest2str(digest, digest_str);

		if (flag_tag) {
			printf("SHA256 (stdin) = %s", digest_str);
		} else {
			printf("%s %c-", digest_str, flag_binary ? '*' : ' ');
		}

		putchar(flag_zero ? '\0' : '\n');
	}

	return exit_status;
}

/**
 * @brief Main function
 * Parses command-line arguments, handles help requests, and dispatches
 * to either check mode or compute mode.
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit status
 */
int sha256sum_main(int argc, char *argv[]) {
	// Parse command-line arguments
	if (parse_args(argc, argv) != 0) {
		return 1;
	}

	// Show help and exit if requested
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
REGISTER_MODULE(sha256sum);
