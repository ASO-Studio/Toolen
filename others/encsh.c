/**
 *	encsh.c - Encrypt a shell script
 *
 * 	Created by YangZlib
 *	Modified by RoofAlan
 *		2025/8/11
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "module.h"
#include "config.h"

// Convert hex character to 4-bit value (0-15), 0xff for invalid
static unsigned char hex_to_byte(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	return 0xff;
}

// Convert binary data to hex string
static char *bin2hex(const unsigned char *bin, size_t len) {
	if (!bin || len == 0) return NULL;
	char *hex = (char*)malloc(len * 2 + 1);
	if (!hex) return NULL;
	const char *hex_chars = "0123456789abcdef";
	for (size_t i = 0; i < len; i++) {
		hex[2*i] = hex_chars[(bin[i] >> 4) & 0x0f];
		hex[2*i + 1] = hex_chars[bin[i] & 0x0f];
	}
	hex[len * 2] = '\0';
	return hex;
}

// Read file content into buffer (returns buffer, out_len = file size)
static unsigned char *read_file(const char *path, size_t *out_len) {
	int f = open(path, O_RDONLY);
	if (f < 0) {
		perror("Failed to open file");
		*out_len = 0;
		return NULL;
	}

	// Get file size
	struct stat st;
	if (fstat(f, &st) != 0) {
		perror("Failed to get file size");
		close(f);
		*out_len = 0;
		return NULL;
	}
	size_t file_size = st.st_size;
	if (file_size == 0) {
		fprintf(stderr, "Error: File is empty\n");
		close(f);
		*out_len = 0;
		return NULL;
	}

	// Allocate buffer and read content
	unsigned char *buf = (unsigned char*)malloc(file_size);
	if (!buf) {
		perror("Memory allocation failed");
		close(f);
		*out_len = 0;
		return NULL;
	}

	size_t bytes_read = read(f,  buf, file_size);
	if (bytes_read != file_size) {
		fprintf(stderr, "Error: Incomplete read (expected %zu, got %zu)\n", file_size, bytes_read);
		free(buf);
		close(f);
		*out_len = 0;
		return NULL;
	}

	close(f);
	*out_len = file_size;
	return buf;
}

// Encrypt file content with XOR and return hex string
static char *encrypt_file(const char *file_path, const char *key_hex, size_t *file_size) {
	// Read file content
	unsigned char *file_data = read_file(file_path, file_size);
	if (!file_data || *file_size == 0) return NULL;

	// Validate key (must be 2 hex chars)
	if (strlen(key_hex) != 2) {
		fprintf(stderr, "Error: Key must be exactly 2 hex characters (e.g., 'a1')\n");
		free(file_data);
		return NULL;
	}
	unsigned char b1 = hex_to_byte(key_hex[0]);
	unsigned char b2 = hex_to_byte(key_hex[1]);
	if (b1 == 0xff || b2 == 0xff) {
		fprintf(stderr, "Error: Key contains invalid hex characters (0-9, a-f, A-F only)\n");
		free(file_data);
		return NULL;
	}
	unsigned char key = (b1 << 4) | b2;

	// XOR encryption
	for (size_t i = 0; i < *file_size; i++) {
		file_data[i] ^= key;
	}

	// Convert to hex string
	char *hex_str = bin2hex(file_data, *file_size);
	free(file_data);
	return hex_str;
}

// Generate decryption executor (runs decrypted content via pipe with $SHELL)
static void generate_decryptor(const char *encrypted_hex, const char *key_hex, const char *output_file) {
	FILE *f = fopen(output_file, "w");
	if (!f) {
		perror("Failed to create decryptor file");
		return;
	}

	// Write decryptor code with embedded encrypted data and key
	fprintf(f, "#include <unistd.h>\n");
	fprintf(f, "#include <sys/wait.h>\n");
	fprintf(f, "#include <stdio.h>\n");
	fprintf(f, "#include <stdlib.h>\n");
	fprintf(f, "#include <string.h>\n");
	fprintf(f, "#include <ctype.h>\n\n");

	fprintf(f, "unsigned char hex_to_byte(char c) {\n");
	fprintf(f, "	if (c >= '0' && c <= '9') return c - '0';\n");
	fprintf(f, "	if (c >= 'a' && c <= 'f') return 10 + c - 'a';\n");
	fprintf(f, "	if (c >= 'A' && c <= 'F') return 10 + c - 'A';\n");
	fprintf(f, "	return 0xff;\n");
	fprintf(f, "}\n\n");

	fprintf(f, "unsigned char *hex2bin(const char *hex, size_t *out_len) {\n");
	fprintf(f, "	if (!hex || *hex == '\\0') { *out_len = 0; return NULL; }\n");
	fprintf(f, "	size_t hex_len = strlen(hex);\n");
	fprintf(f, "	size_t bin_len = hex_len / 2;\n");
	fprintf(f, "	unsigned char *bin = (unsigned char*)malloc(bin_len);\n");
	fprintf(f, "	if (!bin) { *out_len = 0; return NULL; }\n");
	fprintf(f, "	for (size_t i = 0; i < bin_len; i++) {\n");
	fprintf(f, "		unsigned char h = hex_to_byte(hex[2*i]);\n");
	fprintf(f, "		unsigned char l = hex_to_byte(hex[2*i + 1]);\n");
	fprintf(f, "		if (h == 0xff || l == 0xff) { free(bin); *out_len = 0; return NULL; }\n");
	fprintf(f, "		bin[i] = (h << 4) | l;\n");
	fprintf(f, "	}\n");
	fprintf(f, "	*out_len = bin_len; return bin;\n");
	fprintf(f, "}\n\n");

	fprintf(f, "int main() {\n");
	fprintf(f, "	// Encrypted data and key\n");
	fprintf(f, "	const char hex_str[] = \"%s\";\n", encrypted_hex);
	fprintf(f, "	const char key_hex[] = \"%s\";\n", key_hex);
	fprintf(f, "	size_t bin_len;\n");
	fprintf(f, "	unsigned char *decrypted_data;\n");
	fprintf(f, "	unsigned char key;\n\n");

	// Parse key
	fprintf(f, "	// Parse XOR key\n");
	fprintf(f, "	if (strlen(key_hex) != 2) { fprintf(stderr, \"Invalid key length\\n\"); return 1; }\n");
	fprintf(f, "	unsigned char b1 = hex_to_byte(key_hex[0]);\n");
	fprintf(f, "	unsigned char b2 = hex_to_byte(key_hex[1]);\n");
	fprintf(f, "	if (b1 == 0xff || b2 == 0xff) { fprintf(stderr, \"Invalid key characters\\n\"); return 1; }\n");
	fprintf(f, "	key = (b1 << 4) | b2;\n\n");

	// Decrypt data
	fprintf(f, "	// Decrypt data\n");
	fprintf(f, "	decrypted_data = hex2bin(hex_str, &bin_len);\n");
	fprintf(f, "	if (!decrypted_data || bin_len == 0) { fprintf(stderr, \"Decryption failed\\n\"); return 1; }\n");
	fprintf(f, "	for (size_t i = 0; i < bin_len; i++) decrypted_data[i] ^= key;\n\n");

	// Create pipe to run in-memory
	fprintf(f, "	// Create pipe for in-memory execution\n");
	fprintf(f, "	int pipefd[2];\n");
	fprintf(f, "	if (pipe(pipefd) == -1) { perror(\"pipe failed\"); free(decrypted_data); return 1; }\n\n");

	// Fork and execute with $SHELL
	fprintf(f, "	pid_t pid = fork();\n");
	fprintf(f, "	if (pid == -1) { perror(\"fork failed\"); free(decrypted_data); close(pipefd[0]); close(pipefd[1]); return 1; }\n\n");

	fprintf(f, "	if (pid == 0) { // Child: execute via shell\n");
	fprintf(f, "		close(pipefd[1]); // Close write end\n");
	fprintf(f, "		// Redirect pipe read end to stdin\n");
	fprintf(f, "		if (dup2(pipefd[0], STDIN_FILENO) == -1) {\n");
	fprintf(f, "			perror(\"dup2 failed\"); exit(1);\n");
	fprintf(f, "		}\n");
	fprintf(f, "		close(pipefd[0]);\n\n");

	// Get shell from $SHELL (fallback to /bin/sh)
	fprintf(f, "		// Get shell from $SHELL (fallback to /bin/sh)\n");
	fprintf(f, "		const char *shell = getenv(\"SHELL\");\n");
	fprintf(f, "		if (!shell) shell = \"/bin/sh\";\n\n");

	// Execute shell (reads from pipe)
	fprintf(f, "		// Execute shell to run decrypted content\n");
	fprintf(f, "		execl(shell, shell, (char*)NULL);\n");
	fprintf(f, "		perror(\"execl failed\"); // Only reaches here if execl fails\n");
	fprintf(f, "		exit(1);\n");
	fprintf(f, "	}\n\n");

	fprintf(f, "	// Parent: write decrypted data to pipe\n");
	fprintf(f, "	close(pipefd[0]); // Close read end\n");
	fprintf(f, "	if (write(pipefd[1], decrypted_data, bin_len) != (ssize_t)bin_len) {\n");
	fprintf(f, "		perror(\"write to pipe failed\");\n");
	fprintf(f, "	}\n");
	fprintf(f, "	close(pipefd[1]); // Close write end (signal EOF to shell)\n");
	fprintf(f, "	free(decrypted_data);\n\n");

	// Wait for child
	fprintf(f, "	// Wait for shell to finish\n");
	fprintf(f, "	int status;\n");
	fprintf(f, "	waitpid(pid, &status, 0);\n");
	fprintf(f, "	return WEXITSTATUS(status);\n");
	fprintf(f, "}\n");

	fclose(f);
	printf("Decryptor generated: %s\n", output_file);
}

// Print usage instructions
static void print_help(const char *prog_name) {
	SHOW_VERSION(stdout);
	fprintf(stderr, "Usage: %s <input_file> <xor_key> <output_decryptor>\n", prog_name);
	fprintf(stderr, "  <input_file>: Path to the file to encrypt\n");
	fprintf(stderr, "  <xor_key>: 2-character hex key (0-9, a-f, A-F)\n");
	fprintf(stderr, "  <output_decryptor>: Path for the generated decryptor (e.g., decrypt.c)\n");
}

M_ENTRY(encsh) {
	// Check command-line arguments
	if (argc != 4) {
		print_help(argv[0]);
		return 1;
	}

	const char *file_path = argv[1];
	const char *key_hex = argv[2];
	const char *output_file = argv[3];
	size_t file_size;

	// Encrypt the file
	char *encrypted_hex = encrypt_file(file_path, key_hex, &file_size);
	if (!encrypted_hex) {
		fprintf(stderr, "Encryption failed\n");
		return 1;
	}

	// Generate the decryptor executable code
	generate_decryptor(encrypted_hex, key_hex, output_file);
	free(encrypted_hex);
	printf("\033[1;31mWarning: If your code is not wrapped with '{...}</dev/tty', please wrap it yourself\033[0m\n");
	printf("Success! Compile and run the decryptor:\n");
	printf("  gcc %s -o decryptor -static\n", output_file);
	printf("  ./decryptor\n");	
	return 0;
}

REGISTER_MODULE(encsh);
