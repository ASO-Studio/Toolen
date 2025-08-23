#ifndef CONFIG_H
#define CONFIG_H

#define PROGRAM_NAME "Toolen"
#define VERSION "v0.25Beta"
#define COPYRIGHT "Copyright (C) 2025 RoofAlan"

#ifndef _STDIO_H
# include <stdio.h>
#endif  // _STDIO_H

#ifdef __x86_64__	// x86_64
# define PLATFORM "x86_64"
#elif __aarch64__ || __arm64__	// aarch64
# define PLATFORM "AArch64"
#elif __arm__	// arm32
# define PLATFORM "Arm"
#elif __i386__		// i386
# define PLATFORM "i386"
#endif

#ifndef CCVER
# define "UNKNOWN"
#endif

#define SHOW_VERSION(stream) fprintf(stream, PROGRAM_NAME" "VERSION APPEND " (" CCVER ", "  PLATFORM  ")\n")
#define JUST_VERSION()	printf(PROGRAM_NAME" "VERSION APPEND" "COPYRIGHT"\n");

#endif // CONFIG_H
