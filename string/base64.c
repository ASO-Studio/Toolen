#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include "config.h"
#include "module.h"

#define DEFAULT_WRAP 76  // Default line wrap length for encoding

// Base64 alphabet as defined in RFC 4648
static const char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Decode lookup table: maps ASCII characters to their 6-bit values
// -1 indicates an invalid base64 character
static const int decode_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/**
 * Print help information to stdout
 */
static void print_help() {
	SHOW_VERSION(stdout);
	printf("Usage: base64 [OPTION]... [FILE]\n");
	printf("Base64 encode or decode FILE, or standard input, to standard output.\n\n");
	printf("With no FILE, or when FILE is -, read standard input.\n\n");
	printf("Mandatory arguments to long options are mandatory for short options too.\n");
	printf("  -d, --decode          decode data\n");
	printf("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n");
	printf("  -w, --wrap=COLS       wrap encoded lines after COLS character (default %d).\n", DEFAULT_WRAP);
	printf("                        Use 0 to disable line wrapping\n");
	printf("      --help            display this help and exit\n");
}

/**
 * Encode binary data to Base64 format
 * 
 * @param input  File pointer to read binary data from
 * @param output File pointer to write encoded data to
 * @param wrap   Number of characters per line (0 to disable wrapping)
 * @return 0 on success, -1 on error (I/O error)
 */
static int base64_encode(FILE *input, FILE *output, int wrap) {
	unsigned char buffer[3];  // Read 3 bytes at a time (3*8=24 bits = 4*6 bits)
	ssize_t bytes_read;
	int chars_written = 0;	// Track total encoded characters for line wrapping
	
	while ((bytes_read = fread(buffer, 1, 3, input)) > 0) {
		unsigned char encoded[4];  // 3 bytes encode to 4 Base64 characters
		
		// Encode 3-byte chunks into 4 6-bit values (per RFC 4648)
		encoded[0] = (buffer[0] & 0xFC) >> 2;  // First 6 bits of byte 0
		encoded[1] = ((buffer[0] & 0x03) << 4) | ((bytes_read > 1 ? buffer[1] : 0) >> 4);
		encoded[2] = (bytes_read > 1) ? 
					 ((buffer[1] & 0x0F) << 2) | ((bytes_read > 2 ? buffer[2] : 0) >> 6) : 64;
		encoded[3] = (bytes_read > 2) ? (buffer[2] & 0x3F) : 64;
		
		// Write encoded characters to output
		for (int i = 0; i < 4; i++) {
			if (encoded[i] == 64) {  // Padding character
				fputc('=', output);
			} else {
				fputc(base64_alphabet[encoded[i]], output);
			}
			
			chars_written++;
			
			// Add newline if line wrapping is enabled and limit is reached
			if (wrap > 0 && chars_written % wrap == 0) {
				fputc('\n', output);
			}
		}
	}
	
	// Add final newline if wrapping is enabled and last line isn't complete
	if (wrap > 0 && chars_written > 0 && chars_written % wrap != 0) {
		fputc('\n', output);
	}
	
	// Check for I/O errors
	return ferror(input) || ferror(output) ? -1 : 0;
}

/**
 * Decode Base64 data to binary format
 * 
 * @param input		 File pointer to read encoded data from
 * @param output		File pointer to write binary data to
 * @param ignore_garbage If 1, skip non-base64 characters; if 0, error on them
 * @return 0 on success, -1 on error (invalid input or I/O error)
 */
static int base64_decode(FILE *input, FILE *output, int ignore_garbage) {
	unsigned char buffer[4];  // Read 4 Base64 characters at a time
	int count = 0;			// Number of characters in current buffer
	int c;					// Current character being read
	
	while ((c = fgetc(input)) != EOF) {
		// Skip whitespace and line breaks
		if (isspace(c)) {
			continue;
		}
		
		// Skip non-alphabet characters if ignore_garbage is enabled
		if (ignore_garbage && decode_table[c] == -1 && c != '=') {
			continue;
		}
		
		// Reject invalid characters if not ignoring garbage
		if (decode_table[c] == -1 && c != '=') {
			if (!ignore_garbage) {
				fprintf(stderr, "Invalid base64 character: 0x%02x\n", c);
				return -1;
			}
			continue;
		}
		
		// Add valid character to buffer
		buffer[count++] = (unsigned char)c;
		
		// Decode when buffer is full (4 characters)
		if (count == 4) {
			unsigned char decoded[3];  // 4 base64 chars decode to 3 bytes
			int padding = 0;
			
			// Count padding characters (trailing '=')
			for (int i = 3; i >= 0; i--) {
				if (buffer[i] == '=') {
					padding++;
				} else {
					break;
				}
			}
			
			// Calculate output bytes based on padding
			int output_bytes = 3 - padding;
			
			// Decode 4 base64 characters into 3 bytes
			decoded[0] = (decode_table[buffer[0]] << 2) | (decode_table[buffer[1]] >> 4);
			
			if (output_bytes > 1) {
				decoded[1] = ((decode_table[buffer[1]] & 0x0F) << 4) | (decode_table[buffer[2]] >> 2);
			}
			
			if (output_bytes > 2) {
				decoded[2] = ((decode_table[buffer[2]] & 0x03) << 6) | decode_table[buffer[3]];
			}
			
			// Write decoded bytes to output
			if (fwrite(decoded, 1, output_bytes, output) != output_bytes) {
				return -1;
			}
			
			count = 0;  // Reset buffer for next chunk
		}
	}
	
	// Error on incomplete chunks unless ignoring garbage
	if (count > 0 && !ignore_garbage) {
		fprintf(stderr, "Invalid base64 input length (not a multiple of 4)\n");
		return -1;
	}
	
	// Check for I/O errors
	return ferror(input) || ferror(output) ? -1 : 0;
}

/**
 * Main function: parse arguments and dispatch encode/decode
 */
int base64_main(int argc, char *argv[]) {
	int decode = 0;		  // 1 = decode mode, 0 = encode mode
	int ignore_garbage = 0;  // 1 = skip invalid characters when decoding
	int wrap = DEFAULT_WRAP; // Line wrap length for encoding
	char *input_filename = NULL;  // Input file name (NULL = stdin)
	
	// Long command-line options
	static struct option long_options[] = {
		{"decode", no_argument, 0, 'd'},
		{"ignore-garbage", no_argument, 0, 'i'},
		{"wrap", required_argument, 0, 'w'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	
	// Parse command-line options
	int opt;
	int option_index = 0;
	while ((opt = getopt_long(argc, argv, "diw:hV", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'd':
				decode = 1;
				break;
			case 'i':
				ignore_garbage = 1;
				break;
			case 'w':
				wrap = atoi(optarg);
				if (wrap < 0) {
					fprintf(stderr, "Invalid wrap value: %s\n", optarg);
					return 1;
				}
				break;
			case 'h':
				print_help();
				return 0;
			case '?':
				return 1;
			default:
				abort();  // Shouldn't reach here
		}
	}
	
	// Handle input file argument
	if (optind < argc) {
		input_filename = argv[optind];
		optind++;
	}
	
	// Reject extra arguments
	if (optind < argc) {
		fprintf(stderr, "Too many arguments\n");
		return 1;
	}
	
	// Open input file (or use stdin)
	FILE *input = stdin;
	if (input_filename != NULL && strcmp(input_filename, "-") != 0) {
		input = fopen(input_filename, "rb");
		if (input == NULL) {
			perror("Failed to open input file");
			return 1;
		}
	}
	
	// Execute encode or decode
	int result;
	if (decode) {
		result = base64_decode(input, stdout, ignore_garbage);
	} else {
		result = base64_encode(input, stdout, wrap);
	}
	
	// Cleanup
	if (input != stdin) {
		fclose(input);
	}
	
	return result;
}

REGISTER_MODULE(base64);
