/**
 *	env.c - Print environment or execute command
 *
 * 	Created by RoofAlan
 *		2025/8/3
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#define _GNU_SOURCE // execvpe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "module.h"
#include "config.h"
#include "lib.h"

extern char **environ;

static void env_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr,
		"Usage: env [NAME=VALUE...] [COMMAND] [ARGS]...\n\n"
		"Set environment for command invocation, or list environment variables\n\n"
		"  -i      Clear environment\n"
		"  -u NAME Remove NAME from the environment\n");
}

int env_main(int argc, char **argv) {
	int i = 1;
	char **new_environ = NULL;
	int env_count = 0;

	int opr = 0;
	bool cleanEnv = false;

	while ((opr = getopt(argc, argv, "iu:h")) != -1) {
		switch (opr) {
			case 'i':
				cleanEnv = true;
				break;
			case 'u':
				unsetenv(optarg);
				break;
			case 'h':
				env_show_help();
				return 0;
			case '?':
				fprintf(stderr, "Try pass '--help' for more details\n");
				return 1;
			default:
				abort();
		}
	}

	if (cleanEnv)
		environ = NULL;

	if (environ) {
		for (env_count = 0; environ[env_count]; env_count++);
	}

	// Copy environment
	new_environ = malloc((env_count + 1) * sizeof(char *));
	if (!new_environ) {
		perror("env: malloc");
		return 1;
	}
	
	for (int j = 0; j < env_count; j++) {
		new_environ[j] = strdup(environ[j]);
		if (!new_environ[j]) {
			perror("env: strdup");
			return 1;
		}
	}
	new_environ[env_count] = NULL;

	// Process all environ
	while ((i < argc && strstr(argv[i], "=")) || argv[i][0] == '-') {
		if (argv[i][0] == '-') {
			i++;
			continue;
		}
		const char *var_name = argv[i];
		const char *eq_pos = strchr(var_name, '=');
		size_t name_len = eq_pos - var_name;

		// Check environ exists
		int found = 0;
		for (int j = 0; j < env_count; j++) {
			if (strncmp(new_environ[j], var_name, name_len) == 0 && 
				new_environ[j][name_len] == '=') {
				// Cover orignal environ
				free(new_environ[j]);
				new_environ[j] = strdup(argv[i]);
				if (!new_environ[j]) {
					perror("env: strdup");
					return 1;
				}
				found = 1;
				break;
			}
		}
		
		if (!found) {
			// Add new environ
			char **temp = realloc(new_environ, (env_count + 2) * sizeof(char *));
			if (!temp) {
				perror("env: realloc");
				return 1;
			}
			new_environ = temp;
			
			new_environ[env_count] = strdup(argv[i]);
			if (!new_environ[env_count]) {
				perror("env: strdup");
				return 1;
			}
			env_count++;
			new_environ[env_count] = NULL;
		}
		
		i++;
	}

	// Update global pointer
	environ = new_environ;

	for (; argv[i][0] == '-'; )
		i++;

	if (i < argc && !cleanEnv) {
		// Execute command
		execvp(argv[i], &argv[i]);
		perror("env");
		return 1;
	} else {
		// Print environment
		for (int j = 0; new_environ[j]; j++) {
			printf("%s\n", new_environ[j]);
		}
	}
	
	return 0;
}

REGISTER_MODULE(env);
