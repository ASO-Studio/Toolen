/**
 *	module.c - Module registration
 *
 * 	Created by RoofAlan
 *		2025/8/1
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */
 
#include "lib.h"
#include "module.h"
#include <string.h>
#include <unistd.h>

/* Global module registry header pointer */
static ModApi *module_registry = NULL;

/* Platform-specific segment boundary declarations */
#if defined(__linux__) || defined(__unix__) || defined(__gnu_hurd__)
	// Linux/Unix: Standard GCC Symbol
	extern ModApi __start_module_section[];
	extern ModApi __stop_module_section[];
#elif defined(__APPLE__)
	// macOS: Special symbol
	extern ModApi __start_module_section[] __asm("section$start$__DATA$__module_section__");
	extern ModApi __stop_module_section[] __asm("section$end$__DATA$__module_section__");
#elif defined(_WIN32) && defined(_MSC_VER)
	// Windows MSVC: pragma section + global symbol
	__declspec(allocate("__module_section__")) ModApi *__start_module_section;
	__declspec(allocate("__module_section__")) ModApi *__stop_module_section;
#else
	ModApi *__start_module_section = NULL;
	ModApi *__stop_module_section = NULL;
#endif

// Sort
static void insert_sorted(ModApi *new_module) {
	if (!module_registry || new_module->priority < module_registry->priority) {
		new_module->next = module_registry;
		module_registry = new_module;
		return;
	}
	
	ModApi *current = module_registry;
	while (current->next && current->next->priority <= new_module->priority) {
		current = current->next;
	}

	new_module->next = current->next;
	current->next = new_module;
}

// Initialize function table
void module_registry_init(void) {
	// Already initialized, return
	static int initialized = 0;
	if (initialized) return;
	initialized = 1;
	
	//printf("Initializing module registry...\n");
	
#if !(defined(__linux__) || defined(__APPLE__) || (defined(_WIN32) && defined(_MSC_VER)) || defined(__gnu_hurd__))
	// Unsupport platforms
	printf("WARNING: Manual registration required on this platform\n");
	return;
#endif

	for (ModApi *f = __start_module_section; f < __stop_module_section; f++) {
		// Create a copy
		ModApi *new_module = malloc(sizeof(ModApi));
		if (!new_module) {
			fprintf(stderr, "Memory allocation failed\n");
			exit(1);
		}
		*new_module = *f;
		insert_sorted(new_module);
	}
}

// Print all functions
void list_all_modules(void) {
	module_registry_init();
	int isOutTty = 0;
	if (isatty(STDOUT_FILENO)) {
		isOutTty = 1;
	}

	for (ModApi *f = module_registry; f; f = f->next) {
		printf("%s", f->name);
		if (isOutTty)
			printf(" ");
		else
			printf("\n");
		
	}
	if (isOutTty)
		printf("\n");
}

int find_module(const char *name) {
	module_registry_init();

	for(ModApi *f = module_registry; f; f = f->next) {
		if(strcmp(f->name, name) == 0)
			return 0;
	}
	return 1;
}

// Run a module
int run_module(const char* name, int argc, char **argv) {
	module_registry_init();

	int suc = 1; // Default = 1 (Failed)

	for (ModApi *f = module_registry; f; f = f->next) {
		if(strcmp(name, f->name) == 0){
			suc = 0;
			setProgramName(f->name);
			if(f->main) {
				suc = f->main(argc, argv);
			}
		}
	}
	return suc;
}
