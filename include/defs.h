/*
 * defs.h - Some definitions
 */

#ifndef _DEFS_H
#define _DEFS_H

#if __has_include(<linux/limits.h>)
# include <linux/limits.h>
#endif

#define unalign __attribute__((packed))		// No alignment
#define nonret __attribute__((noreturn))	// No return
#define initary __attribute__((constructor))	// Would be executed before main
#define exitary __attribute__((destructor))	// Would be executed after main
#define aliasof(n) __attribute__((alias(n)))	// Create another name for function n
#define align(n) __attribute__((aligned(n)))	// Memory aligned N
#define non_used __attribute__((unused))	// Would not be used
#define fused __attribute__((used))		// The function would be used
#define weakdef __attribute__((weak))		// Weak symbol definition
#define fthrough __attribute__((fallthrough))	// fall-through

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#endif // _DEFS_H
