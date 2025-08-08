#ifndef CONFIG_H
#define CONFIG_H

#define PROGRAM_NAME "Toolen"
#define VERSION "v0.1Beta"
#define COPYRIGHT "Copyright (C) ASO-Studio"

#ifndef _STDIO_H
# include <stdio.h>
#endif  // _STDIO_H

#define SHOW_VERSION(stream) fprintf(stream, PROGRAM_NAME" "VERSION" "COPYRIGHT"\n")

#endif // CONFIG_H
