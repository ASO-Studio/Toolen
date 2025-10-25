/**
 *	binedl.c - Binary Program Editor/Loader
 *
 * 	Created by RoofAlan
 *		2025/10/25
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#define _GNU_SOURCE	// For mremap
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#include "config.h"
#include "module.h"

#define DEFAULT_SIZE 4096	// Default memory region size (4KB)
#define PROMPT "[0x%04lx] > "	// Prompt format showing current address
#define MAX_INPUT 1024		// Maximum input length
#define MAX_ARGS 1024		// Maximum number of command arguments

// Structure to represent our memory region
typedef struct {
	void *base;	// Base address of mapped memory
	size_t size;	// Current size of memory region in bytes
	size_t offset;	// Current address offset for operations
} MemoryRegion;

// Function pointer type for command handlers
typedef void (*CommandHandler)(MemoryRegion*, char**);

// Forward declarations of command handler functions
void handle_set(MemoryRegion *mem, char **args);
void handle_print(MemoryRegion *mem, char **args);
void handle_write(MemoryRegion *mem, char **args);
void handle_run(MemoryRegion *mem, char **args);
void handle_resize(MemoryRegion *mem, char **args);
void handle_quit(MemoryRegion *mem, char **args);

// Command table: maps command names to handler functions
const struct {
	const char *name;	// Command name (and aliases)
	CommandHandler handler;	// Function to handle the command
} commands[] = {
	{"s", handle_set},		// set offset
	{"set", handle_set},		// alias for set
	{"p", handle_print},		// print memory map
	{"print", handle_print},	// alias for print
	{"w", handle_write},		// write bytes
	{"write", handle_write},	// alias for write
	{"r", handle_run},		// run code
	{"run", handle_run},		// alias for run
	{"c", handle_resize},	// change size
	{"resize", handle_resize},	// alias for resize
	{"q", handle_quit},		// quit
	{"quit", handle_quit},	// alias for quit
	{NULL, NULL}		// End marker
};

// Initialize a memory region using mmap
int init_memory(MemoryRegion *mem, size_t size) {
	// Map memory with Read/Write/Execute permissions
	mem->base = mmap(NULL, size, 
					PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_PRIVATE | MAP_ANONYMOUS, 
					-1, 0);

	if (mem->base == MAP_FAILED) {
		perror("mmap failed");
		return -1;
	}

	mem->size = size;
	mem->offset = 0;  // Start at address 0
	return 0;
}

// Resize the memory region while preserving contents
int resize_memory(MemoryRegion *mem, size_t new_size) {
	// Use mremap to resize the memory region
	void *new_base = mremap(mem->base, mem->size, new_size, MREMAP_MAYMOVE);
	if (new_base == MAP_FAILED) {
		perror("mremap failed");
		return -1;
	}
	
	mem->base = new_base;
	mem->size = new_size;

	// Ensure current offset is within new bounds
	if (mem->offset >= new_size) {
		mem->offset = new_size - 1;
	}

	return 0;
}

// Free the mapped memory region
void free_memory(MemoryRegion *mem) {
	if (mem->base != NULL && mem->base != MAP_FAILED) {
		munmap(mem->base, mem->size);
		mem->base = NULL;
		mem->size = 0;
		mem->offset = 0;
	}
}

// Parse input string into arguments
void parse_input(char *input, char **args) {
	char *token = strtok(input, " ");
	int i = 0;

	// Tokenize input using space as delimiter
	while (token != NULL && i < MAX_ARGS - 1) {
		args[i++] = token;
		token = strtok(NULL, " ");
	}
	args[i] = NULL;  // NULL-terminate the argument list
}

// Execute command based on parsed arguments
void execute_command(MemoryRegion *mem, char **args) {
	if (args[0] == NULL) return;  // Empty command

	// Search command table for matching command
	for (int i = 0; commands[i].name != NULL; i++) {
		if (strcmp(commands[i].name, args[0]) == 0) {
			commands[i].handler(mem, args);
			return;
		}
	}

	printf("Unknown command: %s\n", args[0]);
}

/* Command handler implementations */

// Handle 'set' command: Set current address offset
void handle_set(MemoryRegion *mem, char **args) {
	if (args[1] == NULL) {
		printf("Usage: s(et) [address]\n");
		return;
	}

	// Convert argument to unsigned long
	unsigned long addr = strtoul(args[1], NULL, 0);

	// Validate address range
	if (addr >= mem->size) {
		printf("Error: Address 0x%lx out of range (0-0x%lx)\n", addr, mem->size - 1);
		return;
	}

	mem->offset = addr;
	printf("Current address set to 0x%04lx\n", addr);
}

// Handle 'print' command: Display memory information
void handle_print(MemoryRegion *mem, char **args) {
	(void)args;
	printf("Memory Map:\n");
	printf("  Base address: %p\n", mem->base);
	printf("  Size: 0x%04lx bytes\n", mem->size);
	printf("  Current offset: 0x%04lx\n", mem->offset);

	// Display memory contents around current offset
	printf("\nMemory content around 0x%04lx:\n", mem->offset);

	// Calculate display range (16 bytes before and after)
	size_t start = (mem->offset >= 16) ? mem->offset - 16 : 0;
	size_t end = (mem->offset + 16 < mem->size) ? mem->offset + 16 : mem->size - 1;

	// Print memory in hex format
	for (size_t i = start; i <= end; i++) {
		// New line every 16 bytes with address prefix
		if (i % 16 == 0) {
			printf("\n0x%04lx: ", i);
		}
		// Print byte as two-digit hex
		printf("%02x ", *((unsigned char*)mem->base + i));
	}
	printf("\n");
}

// Handle 'write' command: Write bytes to memory
void handle_write(MemoryRegion *mem, char **args) {
	if (args[1] == NULL) {
		printf("Usage: w(rite) [Byte1] [Byte2] ...\n");
		return;
	}

	int i = 1;
	int bytes_written = 0;
	while (args[i] != NULL) {
		// Check for memory bounds
		if (mem->offset >= mem->size) {
			printf("Error: Memory full. Use 'resize' to expand memory.\n");
			break;
		}
		
		// Convert argument to byte value
		unsigned long byte = strtoul(args[i], NULL, 0);
		
		// Warn if value exceeds byte size
		if (byte > 0xFF) {
			printf("Warning: Truncating value 0x%lx to 0x%02lx\n", byte, byte & 0xFF);
		}

		// Write byte to memory
		*((unsigned char*)mem->base + mem->offset) = (unsigned char)(byte & 0xFF);
		mem->offset++;  // Move to next address
		i++;
		bytes_written++;
	}

	printf("%d bytes written\n", bytes_written);
}

// Handle 'run' command: Execute code in memory
void handle_run(MemoryRegion *mem, char **args) {
	size_t run_offset = mem->offset;  // Default to current offset

	// Use specified address if provided
	if (args[1] != NULL) {
		unsigned long addr = strtoul(args[1], NULL, 0);
		if (addr >= mem->size) {
			printf("Error: Address 0x%lx out of range\n", addr);
			return;
		}
		run_offset = addr;
	}
	
	printf("Executing code at 0x%04lx...\n", run_offset);
	
	// Create function pointer to memory location
	void (*func)() = (void (*)())((char*)mem->base + run_offset);

	// Execute the code
	func();

	printf("Execution completed\n");
}

// Handle 'resize' command: Change memory region size
void handle_resize(MemoryRegion *mem, char **args) {
	if (args[1] == NULL) {
		printf("Usage: c(hange size) [Size]\n");
		return;
	}

	unsigned long new_size = strtoul(args[1], NULL, 0);
	if (new_size == 0) {
		printf("Error: Invalid size\n");
		return;
	}

	if (resize_memory(mem, new_size) == 0) {
		printf("Memory resized to 0x%04lx bytes\n", new_size);
	}
}

// Handle 'quit' command: Clean up and exit
void handle_quit(MemoryRegion *mem, char **args) {
	(void)args;
	free_memory(mem);
	printf("Memory released. Exiting...\n");
	exit(0);
}

M_ENTRY(binedl) {
	(void)argc; (void)argv;
	MemoryRegion mem = {0};  // Initialize to zero

	// Initialize memory mapping
	if (init_memory(&mem, DEFAULT_SIZE) != 0) {
		return 1;  // Exit on failure
	}

	SHOW_VERSION(stdout);
	printf("Binary Program Editor/Loader\n");
	printf("Initialized memory: %p (size: 0x%04lx)\n", mem.base, mem.size);
	printf("Type 'help' for available commands\n\n");

	char input[MAX_INPUT];
	char *args[MAX_ARGS];  // Argument array

	// Main command loop
	while (1) {
		printf(PROMPT, mem.offset);
		fflush(stdout);

		// Read user input
		if (fgets(input, MAX_INPUT, stdin) == NULL) {
			break;  // Exit on EOF (Ctrl+D)
		}

		// Remove trailing newline
		input[strcspn(input, "\n")] = '\0';
		
		if (strlen(input) == 0) {
			continue;  // Skip empty input
		}

		// Handle help command separately
		if (strcmp(input, "help") == 0) {
			printf("Available commands:\n");
			printf("  s(et) [address]    - Set current address\n");
			printf("  p(rint)            - Print memory map and contents\n");
			printf("  w(rite) [b1] [b2]..- Write bytes at current address\n");
			printf("  r(un) [address]    - Execute code at specified or current address\n");
			printf("  c(hange) [size]    - Resize memory region\n");
			printf("  q(uit)             - Quit and release memory\n");
			continue;
		}

		// Parse and execute command
		parse_input(input, args);
		execute_command(&mem, args);
	}

	// Clean up before exit
	free_memory(&mem);
	return 0;
}

REGISTER_MODULE(binedl);
