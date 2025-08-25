#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>

#ifdef DEBUG

#define CFUNC "\033[32;1m"
#define CINFO "\033[36;1m"
#define CRESET "\033[0m"

#define LOG(...) do { \
		fprintf(stderr, "[" CINFO "%-10s" CRESET ": ", __FILE__); \
		fprintf(stderr, CFUNC "%-18s" CRESET "] ", __func__); \
		fprintf(stderr, __VA_ARGS__); \
		fflush(stderr); \
	} while(0)

#else
# define LOG(...)
#endif // DEBUG

#endif // _DEBUG_H
