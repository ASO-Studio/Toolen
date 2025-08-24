/**
 *	sha224sum.c - Check SHA224 checksums
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
 * @def SHA224_DIGEST_SIZE
 * @brief Size of SHA-224 digest in bytes (224 bits = 28 bytes)
 */
#define SHA224_DIGEST_SIZE 28

/**
 * @def SHA224_BLOCK_SIZE
 * @brief Size of SHA-224 processing block in bytes (512 bits = 64 bytes)
 */
#define SHA224_BLOCK_SIZE 64

/**
 * @def SHA224_ROUNDS
 * @brief Number of transformation rounds in SHA-224 (same as SHA-256)
 */
#define SHA224_ROUNDS 64

/**
 * @struct SHA224_CTX
 * @brief Context structure for SHA-224 hashing operations
 * @var SHA224_CTX::state
 * Current hash state (8 x 32-bit registers)
 * @var SHA224_CTX::bit_len
 * Total length of input data in bits (for padding)
 * @var SHA224_CTX::buffer
 * Buffer to accumulate input data until a full block is formed
 */
typedef struct {
	uint32_t state[8];		// Hash state registers (a-h)
	uint64_t bit_len;		 // Total input length in bits
	uint8_t buffer[SHA224_BLOCK_SIZE];  // Input data buffer
} SHA224_CTX;

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
 * @brief SHA-224 initial hash values (FIPS 180-4 specification)
 * Derived from the first 32 bits of the fractional parts of the square roots
 * of the first 8 prime numbers.
 */
static const uint32_t SHA224_INIT_STATE[8] = {
	0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
	0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
};

/**
 * @brief SHA-256 round constants (shared with SHA-224)
 * K[0..63] are the first 32 bits of the fractional parts of the cube roots
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
 * @brief Right rotation helper macro
 * @param x Value to rotate
 * @param n Number of bits to rotate
 * @return Rotated value
 */
#define ROTR(x, n) ((x >> n) | (x << (32 - n)))

/**
 * @brief SHA-256 sigma0 helper function
 * @param x Input 32-bit word
 * @return Transformed value
 */
#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))

/**
 * @brief SHA-256 sigma1 helper function
 * @param x Input 32-bit word
 * @return Transformed value
 */
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))

/**
 * @brief SHA-256 gamma0 helper function
 * @param x Input 32-bit word
 * @return Transformed value
 */
#define GAMMA0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3))

/**
 * @brief SHA-256 gamma1 helper function
 * @param x Input 32-bit word
 * @return Transformed value
 */
#define GAMMA1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10))

/**
 * @brief Ch (choice) helper function
 * @param x, y, z Input 32-bit words
 * @return (x & y) ^ (~x & z)
 */
#define CH(x, y, z) ((x & y) ^ (~x & z))

/**
 * @brief Maj (majority) helper function
 * @param x, y, z Input 32-bit words
 * @return (x & y) ^ (x & z) ^ (y & z)
 */
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

/**
 * @brief Initialize SHA-224 context
 * Resets state to initial values, clears buffer, and resets bit length
 * @param ctx Pointer to SHA224_CTX structure
 */
static void sha224_init(SHA224_CTX *ctx) {
	memcpy(ctx->state, SHA224_INIT_STATE, sizeof(SHA224_INIT_STATE));
	ctx->bit_len = 0;
	memset(ctx->buffer, 0, SHA224_BLOCK_SIZE);
}

/**
 * @brief SHA-224 compression function (core transformation)
 * Processes a 512-bit block and updates the hash state
 * @param ctx Pointer to SHA224_CTX containing current state
 * @param block 64-byte (512-bit) input block
 */
static void sha224_transform(SHA224_CTX *ctx, const uint8_t block[SHA224_BLOCK_SIZE]) {
	uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
	uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
	uint32_t w[64];  // Message schedule array
	uint32_t t1, t2;
	int i;

	// Step 1: Prepare message schedule W[0..63]
	for (i = 0; i < 16; i++) {
		// Convert 4 bytes from block to 32-bit big-endian word
		w[i] = (block[4*i] << 24) | (block[4*i + 1] << 16) |
			   (block[4*i + 2] << 8) | block[4*i + 3];
	}

	for (i = 16; i < 64; i++) {
		// Extend W[16..63] using gamma functions
		w[i] = GAMMA1(w[i-2]) + w[i-7] + GAMMA0(w[i-15]) + w[i-16];
	}

	// Step 2: Compression loop (64 rounds)
	for (i = 0; i < 64; i++) {
		t1 = h + SIGMA1(e) + CH(e, f, g) + SHA256_K[i] + w[i];
		t2 = SIGMA0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	// Step 3: Update state with this block's contribution
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
 * @brief Update SHA-224 context with input data
 * Accumulates data in buffer; processes full blocks when buffer is full
 * @param ctx Pointer to SHA224_CTX structure
 * @param data Input data to hash
 * @param len Length of input data in bytes
 */
static void sha224_update(SHA224_CTX *ctx, const uint8_t *data, size_t len) {
	size_t i, pos = (ctx->bit_len / 8) % SHA224_BLOCK_SIZE;
	ctx->bit_len += (uint64_t)len * 8;  // Update total bit length

	// Process full blocks if buffer + new data exceeds block size
	if (pos + len >= SHA224_BLOCK_SIZE) {
		// Fill remaining space in buffer and process
		memcpy(ctx->buffer + pos, data, (i = SHA224_BLOCK_SIZE - pos));
		sha224_transform(ctx, ctx->buffer);

		// Process remaining full blocks directly from input
		for (; i + SHA224_BLOCK_SIZE <= len; i += SHA224_BLOCK_SIZE) {
			sha224_transform(ctx, data + i);
		}
		pos = 0;  // Reset buffer position
	} else {
		i = 0;  // No full blocks to process
	}

	// Save remaining data to buffer
	memcpy(ctx->buffer + pos, data + i, len - i);
}

/**
 * @brief Finalize SHA-224 hash computation
 * Applies padding, processes final block(s), and extracts 224-bit digest
 * @param digest Output buffer for 28-byte SHA-224 digest
 * @param ctx Pointer to SHA224_CTX structure
 */
static void sha224_final(uint8_t digest[SHA224_DIGEST_SIZE], SHA224_CTX *ctx) {
	size_t pos = (ctx->bit_len / 8) % SHA224_BLOCK_SIZE;

	// Step 1: Add 0x80 padding marker
	ctx->buffer[pos++] = 0x80;

	// Step 2: If insufficient space for 64-bit length, process current block
	if (pos > SHA224_BLOCK_SIZE - 8) {
		memset(ctx->buffer + pos, 0, SHA224_BLOCK_SIZE - pos);
		sha224_transform(ctx, ctx->buffer);
		pos = 0;
	}

	// Step 3: Pad with zeros (reserve 8 bytes for length)
	memset(ctx->buffer + pos, 0, SHA224_BLOCK_SIZE - 8 - pos);

	// Step 4: Append total length (big-endian 64-bit)
	ctx->buffer[SHA224_BLOCK_SIZE - 8] = (uint8_t)(ctx->bit_len >> 56);
	ctx->buffer[SHA224_BLOCK_SIZE - 7] = (uint8_t)(ctx->bit_len >> 48);
	ctx->buffer[SHA224_BLOCK_SIZE - 6] = (uint8_t)(ctx->bit_len >> 40);
	ctx->buffer[SHA224_BLOCK_SIZE - 5] = (uint8_t)(ctx->bit_len >> 32);
	ctx->buffer[SHA224_BLOCK_SIZE - 4] = (uint8_t)(ctx->bit_len >> 24);
	ctx->buffer[SHA224_BLOCK_SIZE - 3] = (uint8_t)(ctx->bit_len >> 16);
	ctx->buffer[SHA224_BLOCK_SIZE - 2] = (uint8_t)(ctx->bit_len >> 8);
	ctx->buffer[SHA224_BLOCK_SIZE - 1] = (uint8_t)ctx->bit_len;

	// Process final block
	sha224_transform(ctx, ctx->buffer);

	// Extract 224-bit digest (first 7 of 8 state registers)
	for (int i = 0; i < 7; i++) {
		digest[4*i] = (uint8_t)(ctx->state[i] >> 24);
		digest[4*i + 1] = (uint8_t)(ctx->state[i] >> 16);
		digest[4*i + 2] = (uint8_t)(ctx->state[i] >> 8);
		digest[4*i + 3] = (uint8_t)ctx->state[i];
	}

	// Clear context for security
	memset(ctx, 0, sizeof(SHA224_CTX));
}

/**
 * @brief Compute SHA-224 hash of a file
 * @param filename Path to file ("-" for standard input)
 * @param digest Output buffer for 28-byte digest
 * @param binary_mode If non-zero, open file in binary mode
 * @return 0 on success, -1 if file can't be opened, -2 on read error
 */
static int file_sha224(const char *filename, uint8_t digest[SHA224_DIGEST_SIZE], int binary_mode) {
	FILE *file = stdin;

	// Open file (or use stdin)
	if (filename && strcmp(filename, "-") != 0) {
		file = fopen(filename, binary_mode ? "rb" : "r");
		if (!file) return -1;  // Open failed
	}

	SHA224_CTX ctx;
	sha224_init(&ctx);
	uint8_t buffer[8192];
	size_t bytes_read;

	// Read and process file content
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		sha224_update(&ctx, buffer, bytes_read);
	}

	// Check for read errors
	int read_err = ferror(file);
	if (file != stdin) fclose(file);
	if (read_err) { errno = read_err; return -2; }

	// Finalize hash
	sha224_final(digest, &ctx);
	return 0;
}

/**
 * @brief Compare two SHA-224 digests
 * @param a First digest
 * @param b Second digest
 * @return 1 if equal, 0 otherwise
 */
static int digest_cmp(const uint8_t a[SHA224_DIGEST_SIZE], const uint8_t b[SHA224_DIGEST_SIZE]) {
	return memcmp(a, b, SHA224_DIGEST_SIZE) == 0;
}

/**
 * @brief Convert SHA-224 digest to hex string (56 lowercase chars)
 * @param digest 28-byte digest
 * @param str Output buffer (must be at least 57 bytes)
 */
static void digest2str(const uint8_t digest[SHA224_DIGEST_SIZE], char *str) {
	static const char hex[] = "0123456789abcdef";
	for (int i = 0; i < SHA224_DIGEST_SIZE; i++) {
		str[2*i] = hex[(digest[i] >> 4) & 0x0F];
		str[2*i + 1] = hex[digest[i] & 0x0F];
	}
	str[2 * SHA224_DIGEST_SIZE] = '\0';
}

/**
 * @brief Convert hex string to SHA-224 digest
 * @param str 56-character hex string
 * @param digest Output buffer for 28-byte digest
 * @return 0 on success, -1 if invalid
 */
static int str2digest(const char *str, uint8_t digest[SHA224_DIGEST_SIZE]) {
	if (strlen(str) != 2 * SHA224_DIGEST_SIZE) return -1;

	for (int i = 0; i < SHA224_DIGEST_SIZE; i++) {
		unsigned int val;
		if (sscanf(&str[2*i], "%2x", &val) != 1) return -1;
		digest[i] = (uint8_t)val;
	}
	return 0;
}

/**
 * @brief Print help message
 */
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: sha224sum [OPTION]... [FILE]...\n");
	printf("Print or check SHA224 (224-bit) checksums.\n\n");
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
 * @param argc Number of arguments
 * @param argv Argument array
 * @return 0 on success, -1 on error
 */
static int parse_args(int argc, char *argv[]) {
	static const struct option long_opts[] = {
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
		{NULL, 0, NULL, 0}
	};

	int opt, idx = 0;
	while ((opt = getopt_long(argc, argv, "bctzw", long_opts, &idx)) != -1) {
		switch (opt) {
			case 'b': flag_binary = 1; flag_text = 0; break;
			case 'c': flag_check = 1; break;
			case 't': flag_text = 1; flag_binary = 0; break;
			case 'z': flag_zero = 1; break;
			case 'w': flag_warn = 1; break;
			case 0:
				if (!strcmp(long_opts[idx].name, "tag")) flag_tag = 1;
				else if (!strcmp(long_opts[idx].name, "ignore-missing")) flag_ignore_missing = 1;
				else if (!strcmp(long_opts[idx].name, "quiet")) flag_quiet = 1;
				else if (!strcmp(long_opts[idx].name, "status")) { flag_status = 1; flag_quiet = 1; }
				else if (!strcmp(long_opts[idx].name, "strict")) flag_strict = 1;
				else if (!strcmp(long_opts[idx].name, "help")) flag_help = 1;
				break;
			case '?': return -1;
			default: abort();
		}
	}

	// Check conflicting options
	if (flag_binary && flag_text) {
		fprintf(stderr, "sha224sum: cannot specify both --binary and --text\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Check mode: verify checksums from files
 * @param argc Number of arguments
 * @param argv Argument array
 * @return Exit status (0 for success)
 */
static int check_mode(int argc, char *argv[]) {
	int exit_status = 0;
	if (optind >= argc) {
		static char stdin_flag[] = "-";
		argv[argc++] = stdin_flag;
	}

	for (int i = optind; i < argc; i++) {
		FILE *checkfile = (!strcmp(argv[i], "-")) ? stdin : fopen(argv[i], "r");
		if (!checkfile) {
			fprintf(stderr, "sha224sum: %s: %s\n", argv[i], strerror(errno));
			exit_status = 1;
			continue;
		}

		char line[1024];
		int line_num = 0;
		while (fgets(line, sizeof(line), checkfile)) {
			line_num++;
			char exp_str[57], filename[1024];
			uint8_t exp_digest[SHA224_DIGEST_SIZE], comp_digest[SHA224_DIGEST_SIZE];
			int ret, bin_mode = 0;
			char mode_char;

			// Process line terminators
			size_t len = strlen(line);
			if (len && line[len-1] == '\n') line[len-1] = '\0';
			else if (!feof(checkfile) && (flag_warn || flag_strict)) {
				fprintf(stderr, "sha224sum: %s:%d: improperly formatted line\n",
						(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
				if (flag_strict) exit_status = 1;
				continue;
			}

			// Parse line (BSD or standard format)
			if (flag_tag) {
				if (sscanf(line, "SHA224 (%s) = %56s", filename, exp_str) != 2) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha224sum: %s:%d: bad BSD format\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) exit_status = 1;
					}
					continue;
				}
			} else {
				if (sscanf(line, "%56s %c%s", exp_str, &mode_char, filename) != 3) {
					if (flag_warn || flag_strict) {
						fprintf(stderr, "sha224sum: %s:%d: bad format\n",
								(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
						if (flag_strict) exit_status = 1;
					}
					continue;
				}
				bin_mode = (mode_char == '*');
			}

			// Validate and convert expected digest
			if (str2digest(exp_str, exp_digest) != 0) {
				if (flag_warn || flag_strict) {
					fprintf(stderr, "sha224sum: %s:%d: invalid checksum\n",
							(checkfile == stdin) ? "(stdin)" : argv[i], line_num);
					if (flag_strict) exit_status = 1;
				}
				continue;
			}

			// Compute and compare digests
			ret = file_sha224(filename, comp_digest, bin_mode || flag_binary);
			if (ret == -1) {  // File missing
				if (!flag_ignore_missing) {
					if (!flag_status) fprintf(stderr, "sha224sum: %s: %s\n", filename, strerror(errno));
					exit_status = 1;
				}
			} else if (ret == -2) {  // Read error
				if (!flag_status) fprintf(stderr, "sha224sum: %s: %s\n", filename, strerror(errno));
				exit_status = 1;
			} else if (!digest_cmp(exp_digest, comp_digest)) {  // Mismatch
				if (!flag_status) fprintf(stderr, "%s: FAILED\n", filename);
				exit_status = 1;
			} else if (!flag_quiet && !flag_status) {  // Match
				printf("%s: OK\n", filename);
			}
		}

		// Check for errors reading checkfile
		if (ferror(checkfile)) {
			fprintf(stderr, "sha224sum: %s: %s\n",
					(checkfile == stdin) ? "(stdin)" : argv[i], strerror(errno));
			exit_status = 1;
		}
		if (checkfile != stdin) fclose(checkfile);
	}

	return exit_status;
}

/**
 * @brief Compute mode: generate checksums for files
 * @param argc Number of arguments
 * @param argv Argument array
 * @return Exit status (0 for success)
 */
static int compute_mode(int argc, char *argv[]) {
	int exit_status = 0;
	int use_stdin = (optind >= argc);

	// Process each file
	for (int i = optind; i < argc; i++) {
		uint8_t digest[SHA224_DIGEST_SIZE];
		char digest_str[57];
		int ret = file_sha224(argv[i], digest, flag_binary);

		if (ret != 0) {
			fprintf(stderr, "sha224sum: %s: %s\n", argv[i], strerror(errno));
			exit_status = 1;
			continue;
		}

		digest2str(digest, digest_str);
		if (flag_tag) printf("SHA224 (%s) = %s", argv[i], digest_str);
		else printf("%s %c%s", digest_str, flag_binary ? '*' : ' ', argv[i]);
		putchar(flag_zero ? '\0' : '\n');
	}

	// Process stdin if no files
	if (use_stdin) {
		uint8_t digest[SHA224_DIGEST_SIZE];
		char digest_str[57];
		int ret = file_sha224("-", digest, flag_binary);
		if (ret != 0) {
			fprintf(stderr, "sha224sum: standard input: %s\n", strerror(errno));
			return 1;
		}

		digest2str(digest, digest_str);
		if (flag_tag) printf("SHA224 (stdin) = %s", digest_str);
		else printf("%s %c-", digest_str, flag_binary ? '*' : ' ');
		putchar(flag_zero ? '\0' : '\n');
	}

	return exit_status;
}

/**
 * @brief Main function: dispatch to compute/check mode
 * @param argc Number of arguments
 * @param argv Argument array
 * @return Exit status
 */
int sha224sum_main(int argc, char *argv[]) {
	if (parse_args(argc, argv) != 0) return 1;
	if (flag_help) { print_help(); return 0; }
	return flag_check ? check_mode(argc, argv) : compute_mode(argc, argv);
}
REGISTER_MODULE(sha224sum);
