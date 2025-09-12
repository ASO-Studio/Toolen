/**
 *	md5sum.c - Check MD5 checksums
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
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include "module.h"
#include "config.h"
/* MD5 context structure */
typedef struct {
	unsigned int state[4];		/* state (ABCD) */
	unsigned int count[2];		/* number of bits, modulo 2^64 */
	unsigned char buffer[64];	 /* input buffer */
} MD5_CTX;

/* Declaration of MD5_Transform function prototype */
static void MD5_Transform(unsigned int state[4], const unsigned char block[64]);

/* MD5 initialization. Begins an MD5 operation, writing a new context. */
static void MD5_Init(MD5_CTX *context) {
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
	context->count[0] = 0;
	context->count[1] = 0;
}

/* F, G, H and I are basic MD5 functions */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left by n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/* FF, GG, HH, and II transformations for rounds 1-4 */
#define FF(a, b, c, d, x, s, ac) { \
	(a) += F((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTATE_LEFT((a), (s)); \
	(a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
	(a) += G((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTATE_LEFT((a), (s)); \
	(a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
	(a) += H((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTATE_LEFT((a), (s)); \
	(a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
	(a) += I((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTATE_LEFT((a), (s)); \
	(a) += (b); \
}

/* MD5 block update operation. Continues an MD5 message-digest
   operation, processing another message block, and updating the
   context. */
static void MD5_Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen) {
	unsigned int i, index, partLen;

	/* Compute number of bytes mod 64 */
	index = (unsigned int)((context->count[0] >> 3) & 0x3F);

	/* Update number of bits */
	if ((context->count[0] += ((unsigned int)inputLen << 3)) < ((unsigned int)inputLen << 3))
		context->count[1]++;
	context->count[1] += ((unsigned int)inputLen >> 29);

	partLen = 64 - index;

	/* Transform as many times as possible */
	if (inputLen >= partLen) {
		memcpy(&context->buffer[index], input, partLen);
		MD5_Transform(context->state, context->buffer);

		for (i = partLen; i + 63 < inputLen; i += 64)
			MD5_Transform(context->state, &input[i]);

		index = 0;
	} else {
		i = 0;
	}

	/* Buffer remaining input */
	memcpy(&context->buffer[index], &input[i], inputLen - i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
   the message digest and zeroizing the context. */
static void MD5_Final(unsigned char digest[16], MD5_CTX *context) {
	unsigned char padding[64] = {0x80};
	unsigned int index, padLen;
	unsigned char bits[8];
	int i;

	/* Save number of bits */
	bits[0] = (unsigned char)((context->count[0]));
	bits[1] = (unsigned char)((context->count[0] >> 8));
	bits[2] = (unsigned char)((context->count[0] >> 16));
	bits[3] = (unsigned char)((context->count[0] >> 24));
	bits[4] = (unsigned char)((context->count[1]));
	bits[5] = (unsigned char)((context->count[1] >> 8));
	bits[6] = (unsigned char)((context->count[1] >> 16));
	bits[7] = (unsigned char)((context->count[1] >> 24));

	/* Pad out to 56 mod 64 */
	index = (unsigned int)((context->count[0] >> 3) & 0x3F);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	MD5_Update(context, padding, padLen);

	/* Append length (before padding) */
	MD5_Update(context, bits, 8);

	/* Store state in digest */
	for (i = 0; i < 4; i++) {
		digest[i] = (unsigned char)(context->state[0] >> (i * 8));
		digest[i + 4] = (unsigned char)(context->state[1] >> (i * 8));
		digest[i + 8] = (unsigned char)(context->state[2] >> (i * 8));
		digest[i + 12] = (unsigned char)(context->state[3] >> (i * 8));
	}

	/* Zeroize sensitive information */
	memset(context, 0, sizeof(*context));
}

/* MD5 basic transformation. Transforms state based on block. */
static void MD5_Transform(unsigned int state[4], const unsigned char block[64]) {
	unsigned int a = state[0], b = state[1], c = state[2], d = state[3];
	unsigned int x[16];
	int i;

	/* Decode input block into x[0..15] */
	for (i = 0; i < 16; i++)
		x[i] = ((unsigned int)block[i * 4]) | (((unsigned int)block[i * 4 + 1]) << 8) |
			   (((unsigned int)block[i * 4 + 2]) << 16) | (((unsigned int)block[i * 4 + 3]) << 24);

	/* Round 1 */
	FF(a, b, c, d, x[0], 7, 0xd76aa478);
	FF(d, a, b, c, x[1], 12, 0xe8c7b756);
	FF(c, d, a, b, x[2], 17, 0x242070db);
	FF(b, c, d, a, x[3], 22, 0xc1bdceee);
	FF(a, b, c, d, x[4], 7, 0xf57c0faf);
	FF(d, a, b, c, x[5], 12, 0x4787c62a);
	FF(c, d, a, b, x[6], 17, 0xa8304613);
	FF(b, c, d, a, x[7], 22, 0xfd469501);
	FF(a, b, c, d, x[8], 7, 0x698098d8);
	FF(d, a, b, c, x[9], 12, 0x8b44f7af);
	FF(c, d, a, b, x[10], 17, 0xffff5bb1);
	FF(b, c, d, a, x[11], 22, 0x895cd7be);
	FF(a, b, c, d, x[12], 7, 0x6b901122);
	FF(d, a, b, c, x[13], 12, 0xfd987193);
	FF(c, d, a, b, x[14], 17, 0xa679438e);
	FF(b, c, d, a, x[15], 22, 0x49b40821);

	/* Round 2 */
	GG(a, b, c, d, x[1], 5, 0xf61e2562);
	GG(d, a, b, c, x[6], 9, 0xc040b340);
	GG(c, d, a, b, x[11], 14, 0x265e5a51);
	GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
	GG(a, b, c, d, x[5], 5, 0xd62f105d);
	GG(d, a, b, c, x[10], 9, 0x02441453);
	GG(c, d, a, b, x[15], 14, 0xd8a1e681);
	GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
	GG(a, b, c, d, x[9], 5, 0x21e1cde6);
	GG(d, a, b, c, x[14], 9, 0xc33707d6);
	GG(c, d, a, b, x[3], 14, 0xf4d50d87);
	GG(b, c, d, a, x[8], 20, 0x455a14ed);
	GG(a, b, c, d, x[13], 5, 0xa9e3e905);
	GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
	GG(c, d, a, b, x[7], 14, 0x676f02d9);
	GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

	/* Round 3 */
	HH(a, b, c, d, x[5], 4, 0xfffa3942);
	HH(d, a, b, c, x[8], 11, 0x8771f681);
	HH(c, d, a, b, x[11], 16, 0x6d9d6122);
	HH(b, c, d, a, x[14], 23, 0xfde5380c);
	HH(a, b, c, d, x[1], 4, 0xa4beea44);
	HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
	HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
	HH(b, c, d, a, x[10], 23, 0xbebfbc70);
	HH(a, b, c, d, x[13], 4, 0x289b7ec6);
	HH(d, a, b, c, x[0], 11, 0xeaa127fa);
	HH(c, d, a, b, x[3], 16, 0xd4ef3085);
	HH(b, c, d, a, x[6], 23, 0x04881d05);
	HH(a, b, c, d, x[9], 4, 0xd9d4d039);
	HH(d, a, b, c, x[12], 11, 0xe6db99e5);
	HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
	HH(b, c, d, a, x[2], 23, 0xc4ac5665);

	/* Round 4 */
	II(a, b, c, d, x[0], 6, 0xf4292244);
	II(d, a, b, c, x[7], 10, 0x432aff97);
	II(c, d, a, b, x[14], 15, 0xab9423a7);
	II(b, c, d, a, x[5], 21, 0xfc93a039);
	II(a, b, c, d, x[12], 6, 0x655b59c3);
	II(d, a, b, c, x[3], 10, 0x8f0ccc92);
	II(c, d, a, b, x[10], 15, 0xffeff47d);
	II(b, c, d, a, x[1], 21, 0x85845dd1);
	II(a, b, c, d, x[8], 6, 0x6fa87e4f);
	II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
	II(c, d, a, b, x[6], 15, 0xa3014314);
	II(b, c, d, a, x[13], 21, 0x4e0811a1);
	II(a, b, c, d, x[4], 6, 0xf7537e82);
	II(d, a, b, c, x[11], 10, 0xbd3af235);
	II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
	II(b, c, d, a, x[9], 21, 0xeb86d391);

	/* Add this chunk's hash to result so far */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

/* Compute MD5 hash of a file */
static int compute_file_md5(const char *filename, unsigned char digest[16], bool binary_mode) {
	MD5_CTX ctx;
	unsigned char buffer[4096];
	ssize_t bytes_read;
	int fd;

	if (strcmp(filename, "-") == 0) {
		fd = STDIN_FILENO;
	} else {
		fd = open(filename, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "md5sum: %s: %s\n", filename, strerror(errno));
			return -1;
		}
	}

	MD5_Init(&ctx);

	while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
		/* On non-GNU systems, text mode would convert line endings here */
		MD5_Update(&ctx, buffer, (unsigned int)bytes_read);
	}

	if (bytes_read == -1) {
		fprintf(stderr, "md5sum: %s: %s\n", filename, strerror(errno));
		if (fd != STDIN_FILENO) close(fd);
		return -1;
	}

	if (fd != STDIN_FILENO) close(fd);

	MD5_Final(digest, &ctx);
	return 0;
}

/* Convert MD5 digest to string */
static void md5_digest_to_string(const unsigned char digest[16], char *str) {
	int i;
	for (i = 0; i < 16; i++) {
		sprintf(str + 2 * i, "%02x", digest[i]);
	}
	str[32] = '\0';
}

/* Check if a line is a valid checksum line */
static bool is_valid_checksum_line(const char *line, char *digest_str, char *filename, char *mode) {
	if (strlen(line) < 34) return false;  /* Minimum length: 32 hex + 1 space + 1 char + 0 */
	
	/* Extract digest */
	strncpy(digest_str, line, 32);
	digest_str[32] = '\0';
	
	/* Check digest is valid hex */
	for (int i = 0; i < 32; i++) {
		if (!isxdigit((unsigned char)digest_str[i])) {
			return false;
		}
	}
	
	/* Extract mode */
	*mode = line[32];
	if (*mode != ' ' && *mode != '*') {
		return false;
	}
	
	/* Extract filename */
	if (line[33] != ' ') {
		return false;
	}
	strcpy(filename, &line[34]);
	
	return true;
}

/* Convert string to MD5 digest */
static bool md5_string_to_digest(const char *str, unsigned char digest[16]) {
	if (strlen(str) != 32) return false;
	
	for (int i = 0; i < 16; i++) {
		char hex[3] = {str[2*i], str[2*i+1], '\0'};
		unsigned int val;
		if (sscanf(hex, "%x", &val) != 1) {
			return false;
		}
		digest[i] = (unsigned char)val;
	}
	return true;
}

/* Compare two MD5 digests */
static bool md5_digests_equal(const unsigned char a[16], const unsigned char b[16]) {
	for (int i = 0; i < 16; i++) {
		if (a[i] != b[i]) return false;
	}
	return true;
}

/* Check checksums from a file */
static int check_checksums(const char *filename, bool ignore_missing, bool quiet, bool status, bool strict, bool warn) {
	FILE *file;
	char line[1024];
	int line_num = 0;
	int errors = 0;
	bool has_errors = false;

	if (strcmp(filename, "-") == 0) {
		file = stdin;
	} else {
		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "md5sum: %s: %s\n", filename, strerror(errno));
			return 1;
		}
	}

	while (fgets(line, sizeof(line), file)) {
		line_num++;
		size_t len = strlen(line);
		if (len > 0 && line[len-1] == '\n') {
			line[len-1] = '\0';  /* Remove newline */
		}

		char expected_digest_str[33];
		char file_to_check[1024];
		char mode;

		if (!is_valid_checksum_line(line, expected_digest_str, file_to_check, &mode)) {
			if (strict) {
				has_errors = true;
				errors++;
			}
			if (warn || strict) {
				fprintf(stderr, "md5sum: %s: %d: improperly formatted checksum line\n", filename, line_num);
			}
			if (status) continue;
			continue;
		}

		/* Check if file exists */
		struct stat st;
		if (stat(file_to_check, &st) != 0) {
			if (!ignore_missing) {
				has_errors = true;
				errors++;
				if (!status) {
					fprintf(stderr, "md5sum: %s: No such file or directory\n", file_to_check);
				}
			}
			continue;
		}

		/* Compute actual digest */
		unsigned char actual_digest[16];
		unsigned char expected_digest[16];
		
		if (compute_file_md5(file_to_check, actual_digest, mode == '*') != 0) {
			has_errors = true;
			errors++;
			continue;
		}
		
		if (!md5_string_to_digest(expected_digest_str, expected_digest)) {
			if (strict) {
				has_errors = true;
				errors++;
			}
			if (warn || strict) {
				fprintf(stderr, "md5sum: %s: %d: invalid hex in checksum\n", filename, line_num);
			}
			continue;
		}

		/* Compare digests */
		if (md5_digests_equal(actual_digest, expected_digest)) {
			if (!quiet && !status) {
				printf("%s: OK\n", file_to_check);
			}
		} else {
			has_errors = true;
			errors++;
			if (!status) {
				printf("%s: FAILED\n", file_to_check);
			}
		}
	}

	if (file != stdin) fclose(file);

	if (has_errors && !status) {
		printf("md5sum: WARNING: %d computed checksum did NOT match\n", errors);
	}

	return has_errors ? 1 : 0;
}


/* Print help information */
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: md5sum [OPTION]... [FILE]...\n");
	printf("Print or check MD5 (128-bit) checksums.\n\n");
	printf("With no FILE, or when FILE is -, read standard input.\n");
	printf("  -b, --binary     read in binary mode\n");
	printf("  -c, --check      read checksums from the FILEs and check them\n");
	printf("    --tag          create a BSD-style checksum\n");
	printf("  -t, --text       read in text mode (default)\n");
	printf("  -z, --zero       end each output line with NUL, not newline,\n");
	printf("                   and disable file name escaping\n\n");
	printf("The following five options are useful only when verifying checksums:\n");
	printf("    --ignore-missing  don't fail or report status for missing files\n");
	printf("    --quiet        don't print OK for each successfully verified file\n");
	printf("    --status       don't output anything, status code shows success\n");
	printf("    --strict       exit non-zero for improperly formatted checksum lines\n");
	printf("  -w, --warn       warn about improperly formatted checksum lines\n\n");
	printf("    --help         display this help and exit\n");
}

M_ENTRY(md5sum) {
	bool binary_mode = false;
	bool check_mode = false;
	bool tag_mode = false;
	bool zero_terminated = false;
	bool ignore_missing = false;
	bool quiet = false;
	bool status = false;
	bool strict = false;
	bool warn = false;
	bool help = false;

	/* Command line options */
	static struct option long_options[] = {
		{"binary", no_argument, 0, 'b'},
		{"check", no_argument, 0, 'c'},
		{"tag", no_argument, 0, 0},
		{"text", no_argument, 0, 't'},
		{"zero", no_argument, 0, 'z'},
		{"ignore-missing", no_argument, 0, 0},
		{"quiet", no_argument, 0, 0},
		{"status", no_argument, 0, 0},
		{"strict", no_argument, 0, 0},
		{"warn", no_argument, 0, 'w'},
		{"help", no_argument, 0, 0},
		{0, 0, 0, 0}
	};

	int opt;
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, "bctzw", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'b':
				binary_mode = true;
				break;
			case 'c':
				check_mode = true;
				break;
			case 't':
				binary_mode = false;
				break;
			case 'z':
				zero_terminated = true;
				break;
			case 'w':
				warn = true;
				break;
			case 0:
				if (strcmp(long_options[option_index].name, "tag") == 0) {
					tag_mode = true;
				} else if (strcmp(long_options[option_index].name, "ignore-missing") == 0) {
					ignore_missing = true;
				} else if (strcmp(long_options[option_index].name, "quiet") == 0) {
					quiet = true;
				} else if (strcmp(long_options[option_index].name, "status") == 0) {
					status = true;
				} else if (strcmp(long_options[option_index].name, "strict") == 0) {
					strict = true;
				} else if (strcmp(long_options[option_index].name, "help") == 0) {
					help = true;
				}
				break;
			default:
				print_help();
				return 1;
		}
	}

	/* Handle help */
	if (help) {
		print_help();
		return 0;
	}

	/* Determine files to process */
	int num_files = argc - optind;
	char **files = argv + optind;

	/* If no files specified, read from stdin */
	if (num_files == 0) {
		num_files = 1;
		static char *stdin_file[] = {"-"};
		files = stdin_file;
	}

	if (check_mode) {
		/* Check mode: verify checksums */
		int result = 0;
		for (int i = 0; i < num_files; i++) {
			int res = check_checksums(files[i], ignore_missing, quiet, status, strict, warn);
			if (res != 0) result = res;
		}
		return result;
	} else {
		/* Generate mode: compute and print checksums */
		for (int i = 0; i < num_files; i++) {
			unsigned char digest[16];
			if (compute_file_md5(files[i], digest, binary_mode) != 0) {
				return 1;
			}

			char digest_str[33];
			md5_digest_to_string(digest, digest_str);

			if (tag_mode) {
				/* BSD-style output */
				printf("MD5 (%s) = %s", files[i], digest_str);
			} else {
				/* Default output */
				printf("%s %c%s", digest_str, binary_mode ? '*' : ' ', files[i]);
			}

			if (zero_terminated) {
				putchar('\0');
			} else {
				putchar('\n');
			}
		}
		return 0;
	}
}
REGISTER_MODULE(md5sum);
