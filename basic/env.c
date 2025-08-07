#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "module.h"
#include "config.h"

extern char **environ;

static int has_equal(const char *str) {
	for (; *str; str++) {
		if (*str == '=') {
			return 1;
		}
	}
	return 0;
}

int env_main(int argc, char **argv) {
	int i = 1;
	char **new_environ = NULL;
	int env_count = 0;

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
	while (i < argc && has_equal(argv[i])) {
		const char *var_name = argv[i];
		const char *eq_pos = strchr(var_name, '=');
		size_t name_len = eq_pos - var_name;
		
		// Check environ exsits
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

	if (i < argc) {
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
