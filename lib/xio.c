/*
 * xio.c - Wrap of I/O functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "debug.h"

// I/O block node structure
typedef struct io_block {
	enum { FILE_TYPE, FD_TYPE } type;
	union {
		FILE *fp;
		int fd;
	} handle;
	struct io_block *next;
} io_block_t;

static io_block_t *io_list = NULL;

// Add I/O block to linked list
static void add_block_file(FILE *fp) {
	LOG("Adding file: %p\n", fp);
	io_block_t *new_block = (io_block_t *)malloc(sizeof(io_block_t));
	if (!new_block) {
		fprintf(stderr, "Fatal error: Cannot allocate memory for management structure\n");
		exit(EXIT_FAILURE);
	}

	new_block->type = FILE_TYPE;
	new_block->handle.fp = fp;
	new_block->next = io_list;
	io_list = new_block;
}

// Add file descriptor block to linked list
static void add_block_fd(int fd) {
	LOG("Adding fd: %d\n", fd);
	io_block_t *new_block = (io_block_t *)malloc(sizeof(io_block_t));
	if (!new_block) {
		fprintf(stderr, "Fatal error: Cannot allocate memory for management structure\n");
		exit(EXIT_FAILURE);
	}

	new_block->type = FD_TYPE;
	new_block->handle.fd = fd;
	new_block->next = io_list;
	io_list = new_block;
}

// Remove file block from linked list
static void remove_block_file(FILE *fp) {
	LOG("Closing file: %p\n", fp);
	io_block_t **curr = &io_list;

	while (*curr) {
		if ((*curr)->type == FILE_TYPE && (*curr)->handle.fp == fp) {
			io_block_t *to_close = *curr;
			*curr = (*curr)->next;
			fclose(to_close->handle.fp);
			free(to_close);
			return;
		}
		curr = &(*curr)->next;
	}
}

// Remove file descriptor block from linked list
static void remove_block_fd(int fd) {
	LOG("Closing fd: %d\n", fd);
	io_block_t **curr = &io_list;

	while (*curr) {
		if ((*curr)->type == FD_TYPE && (*curr)->handle.fd == fd) {
			io_block_t *to_close = *curr;
			*curr = (*curr)->next;
			close(to_close->handle.fd);
			free(to_close);
			return;
		}
		curr = &(*curr)->next;
	}
}

// Clean up all I/O blocks
static void cleanup_all(void) {
	LOG("Cleaning up...\n");
	io_block_t *curr = io_list;

#ifdef DEBUG
	if (!curr) {
		LOG("Nothing to do\n");
	}
#endif

	while (curr) {
		LOG("-> type: %s, handle: %p\n", 
			curr->type == FILE_TYPE ? "FILE" : "FD",
			curr->type == FILE_TYPE ? (void*)curr->handle.fp : (void*)(long)curr->handle.fd);
		
		if (curr->type == FILE_TYPE) {
			fclose(curr->handle.fp);
		} else {
			close(curr->handle.fd);
		}
		
		io_block_t *to_free = curr;
		curr = curr->next;
		free(to_free);
	}
	io_list = NULL;
}

// Handle open failure
static void open_failed(void) {
	fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
	cleanup_all();
	exit(EXIT_FAILURE);
}

// Handle allocation failure
static void allocation_failed(void) {
	fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
	cleanup_all();
	exit(EXIT_FAILURE);
}

// Encapsulated fopen
FILE *xfopen(const char *filename, const char *mode) {
	FILE *fp = fopen(filename, mode);
	if (!fp) {
		open_failed();
	}
	add_block_file(fp);
	return fp;
}

// Encapsulated fclose
void xfclose(FILE *fp) {
	if (fp) {
		remove_block_file(fp);
	}
}

// Encapsulated open
int xopen(const char *pathname, int flags, mode_t mode) {
	int fd = open(pathname, flags, mode);
	if (fd < 0) {
		open_failed();
	}
	add_block_fd(fd);
	return fd;
}

// Encapsulated open with variable arguments (for mode)
int xopen2(const char *pathname, int flags) {
	int fd = open(pathname, flags);
	if (fd < 0) {
		open_failed();
	}
	add_block_fd(fd);
	return fd;
}

// Encapsulated close
void xclose(int fd) {
	if (fd >= 0) {
		remove_block_fd(fd);
	}
}

// Initialization function
__attribute__((constructor))
static void init_xio(void) {
	LOG("initializing...\n");
	atexit(cleanup_all);
	LOG("success\n");
}
