/*
 * module.h - Module registration
 * Support Platforms: Linux/macOS/Windows/Hurd (GCC/Clang/MSVC)
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ModApi {
	const char *name;	// Function name
	int (*main)(int,char**);// Main function
	int priority;
	struct ModApi *next;	// Pointer

} ModApi;

#if defined(__GNUC__) || defined(__clang__)
	// GCC/Clang use section atrribute
	#define FEATURE_SECTION __attribute__((section("module_section"), used))
#elif defined(_MSC_VER)
	// MSVC use pragma section + allocate
	#pragma section("module_section", read)
	#define FEATURE_SECTION __declspec(allocate("module_section"))
#else
	// Other compilers don't support
	#define FEATURE_SECTION
	#warning "Unsupported compiler, using basic registration"
#endif

/**
 * Module registration macro
 * @param module_name (need function 'module_name_main')
 */
#define REGISTER_MODULE(module_name) \
	static ModApi _module_##module_name FEATURE_SECTION = { \
		.name = #module_name, \
		.main = module_name##_main, \
		.priority = 0,	\
		.next = NULL \
	}

/**
 * Module registration macro 2
 * @param module_name (need function 'module_name_main')
 * @param mname (need a string name for this module)
 */
#define REGISTER_MODULE2(module, mname) \
	static ModApi _module2_##module FEATURE_SECTION = { \
		.name = mname , \
		.main = module##_main, \
		.priority = 0,	\
		.next = NULL \
	}

/**
 * Module entry macro
 * @param name (Would expend to 'name_main')
 */
#define M_ENTRY(name) int name##_main(int argc, char *argv[])	// It may not be used :(

// Redirect
#define REDIRECT(src, target) \
	int target##_main (int argc, char *argv[]) {\
		return src##_main(argc, argv); \
	} \
	REGISTER_MODULE(target)

// Initialize
void module_registry_init(void);

// List all modules
void list_all_modules(void);

// Find module
int find_module(const char*);

// Execute module
int run_module(const char*, int, char**);

#ifdef __cplusplus
}
#endif
