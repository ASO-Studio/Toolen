/*
 * timeConvert.c - convert time expression to seconds
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// Convert time expression to seconds
unsigned long timeToSeconds(const char *str) {
	if (str == NULL || *str == '\0') return 0;
	
	char *endptr;
	double value = strtod(str, &endptr);  // Parse numeric part
	
	// Skip whitespace after number
	while (isspace((unsigned char)*endptr)) endptr++;
	
	// Determine conversion factor based on unit
	double factor = 1.0;  // Default: seconds
	switch (*endptr) {
		case 'm': case 'M': factor = 60.0; break;	// Minutes
		case 'h': case 'H': factor = 3600.0; break;	// Hours
		case 'd': case 'D': factor = 86400.0; break;	// Days
		// No unit defaults to seconds
	}
	
	// Calculate total seconds and round to nearest integer
	double total = value * factor;
	return (unsigned long)(total + 0.5);
}
