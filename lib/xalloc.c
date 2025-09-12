/*
 * xalloc.c - Wrap of *alloc functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "debug.h"

// Memory block node structure
typedef struct mem_block {
	void *ptr;
	struct mem_block *next;
} mem_block_t;

static mem_block_t *mem_list = NULL;

// Add memory block to linked list
static void add_block(void *ptr) {
	LOG("Adding: %p\n", ptr);
	mem_block_t *new_block = (mem_block_t *)malloc(sizeof(mem_block_t));
	if (!new_block) {
		// If even the management node allocation fails, exit directly
		fprintf(stderr, "Fatal error: Cannot allocate memory for management structure\n");
		exit(EXIT_FAILURE);
	}

	new_block->ptr = ptr;
	new_block->next = mem_list;
	mem_list = new_block;
}

// Remove memory block from linked list
static void remove_block(void *ptr) {
	LOG("Removing: %p\n", ptr);
	mem_block_t **curr = &mem_list;
	mem_block_t *prev = NULL;

	while (*curr) {
		if ((*curr)->ptr == ptr) {
			mem_block_t *to_free = *curr;

			// Update the list pointers
			if (prev) {
				prev->next = to_free->next;
			} else {
				mem_list = to_free->next;
			}

			free(to_free);
			return;
		}
		prev = *curr;
		curr = &(*curr)->next;
	}

	LOG("Block %p not found in management list\n", ptr);
}

// Clean up all memory blocks
static void cleanup_all(void) {
	LOG("Cleaning up...\n");
	mem_block_t *curr = mem_list;

#ifdef DEBUG
	// If curr is NULL, means nothing to do
	if (!curr) {
		LOG("Nothing to do\n");
	}
#endif

	while (curr) {
		LOG("-> %p\n", curr->ptr);
		free(curr->ptr);
		mem_block_t *to_free = curr;
		curr = curr->next;
		free(to_free);
	}
	mem_list = NULL;
}

// Handle allocation failure
static void allocation_failed(void) {
	fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
	cleanup_all();
	exit(EXIT_FAILURE);
}

// Encapsulated malloc
void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (!ptr) {
		allocation_failed();
	}
	add_block(ptr);
	return ptr;
}

// Encapsulated calloc
void *xcalloc(size_t num, size_t size) {
	void *ptr = calloc(num, size);
	if (!ptr) {
		allocation_failed();
	}
	add_block(ptr);
	return ptr;
}

// Encapsulated realloc
void *xrealloc(void *ptr, size_t size) {
	// If ptr is NULL, equivalent to malloc
	if (!ptr) {
		return xmalloc(size);
	}
	
	void *new_ptr = realloc(ptr, size);
	if (!new_ptr) {
		allocation_failed();
	}
	
	// Update pointer in linked list
	mem_block_t *curr = mem_list;
	while (curr) {
		if (curr->ptr == ptr) {
			curr->ptr = new_ptr;
			break;
		}
		curr = curr->next;
	}
	
	return new_ptr;
}

// Encapsulated free
void xfree(void *ptr) {
	if (ptr) {
		remove_block(ptr);
		free(ptr);
	}
}

// Encapsulated strdup
char *xstrdup(const char *s) {
	char *new_str = strdup(s);
	if (!new_str) {
		allocation_failed();
	}
	add_block(new_str);
	return new_str;
}

// Initialization function
__attribute__((constructor))
static void init_xalloc(void) {
	LOG("initializing XALLOC...\n");
	// Register cleanup function to be called automatically on program exit
	atexit(cleanup_all);
	LOG("success\n");
}
