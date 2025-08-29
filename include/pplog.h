/*
 * pplog.h - Program error output interface header file
 */

#ifndef _PPLOG_H
#define _PPLOG_H

/* Flags */
#define P_ERRNO (1)	// Show errno
#define P_HELP	(1<<2)	// Show "Try pass '--help'..."
#define P_NAME	(1<<3)	// Show program name

int pplog(int flags, const char *msg, ...);

#endif // _PPLOG_H
