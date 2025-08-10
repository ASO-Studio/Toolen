#ifndef CONFIG_H
#define CONFIG_H

#define PROGRAM_NAME "Toolen"
#define VERSION "v0.2Beta"
#define COPYRIGHT "Copyright (C) 2025 RoofAlan"

#ifndef _STDIO_H
# include <stdio.h>
#endif  // _STDIO_H

#define SHOW_VERSION(stream) fprintf(stream, PROGRAM_NAME" "VERSION" "COPYRIGHT"\n")
#define JUST_VERSION()	printf(PROGRAM_NAME" "VERSION"\n");

#define COMPILE_LICENSE

#endif // CONFIG_H
