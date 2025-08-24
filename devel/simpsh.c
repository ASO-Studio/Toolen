/**
 *	simpsh.c - Simple shell program
 *
 * 	Designed by BeiYe
 * 	Modified by RoofAlan
 *		2025/8/17
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>

#include "config.h"
#include "module.h"
#include "lib.h"

#define UNSIG(x) ((unsigned char*)x)

static void sprint(const char *str) {
	write(1, str, strlen(str));
}

static void eprint(const char *str) {
	fprintf(stderr, "%s: %s\n", str, strerror(errno));
}

static void itoa(unsigned int num,char *buffer){
	if(num == 0){
		buffer[0] = '0';
		buffer[1] = '\0';
		return;
	}
	unsigned int temp = num;
	int count = 0;
	while(temp > 0){
		count++;
		temp /= 10;
	}
	for(int i = count-1; i >= 0; i--){
		buffer[i] = (num % 10) + '0';
		num /= 10;
	}
	buffer[count] = '\0';
}

static bool scmp(const char* d, const char *s) {
	if(!d||!s)
		return false;

	if(strlen(d) != strlen(s))
		return false;

	while(*d && *s) {
		if(*d++ != *s++)
			return false;
	}
	return true;
}

static void padz(void* m, size_t len) {
	for(size_t i=0; i<len; i++) {
		UNSIG(m)[i] = 0;
	}
}

// Allocate memory
static void *fmalloc(size_t size) {
	void *p = malloc(size);
	if(!p) {
		eprint("spsh: malloc failed");
		exit(1);
	}
	return p;
}

// Free memory
static void ffree(void *ptr) {
	free(ptr);
}

static bool is_delimiter(char c, const char *delimiters) {
	for (size_t i = 0; delimiters[i] != '\0'; i++) {
		if (c == delimiters[i]) {
			return true;
		}
	}
	return false;
}

// Split command
static char** parse_command(const char *input, const char *delimiters) {
	char **tokens = fmalloc(32 * sizeof(char*));
	char tokenBuffer[2048];
	int token_count = 0;
	int capacity = 32;
	int state = 0;
	int esc = 0;
	size_t buf_index = 0;
	size_t start = 0;

	padz(tokenBuffer, sizeof(tokenBuffer));
	padz(tokens, capacity * sizeof(char*));

	while (input[start] != '\0' && is_delimiter(input[start], delimiters)) {
		start++;
	}

	for (size_t i = start; input[i] != '\0'; i++) {
		char c = input[i];
		if (esc) {
			if (buf_index < sizeof(tokenBuffer) - 1) {
				tokenBuffer[buf_index++] = c;
			}
			esc = 0;
		} else {
			switch(state) {
				case 0:
					if (c == '\\') {
						esc = 1;
					} else if (c == '"') {
						state = 1;
					} else if (c == '\'') {
						state = 2;
					} else if (is_delimiter(c, delimiters)) {
						if (buf_index > 0) {
							tokenBuffer[buf_index] = '\0';
							if(token_count >= capacity) {
								capacity *= 2;
								char **newTokens = fmalloc(capacity * sizeof(char*));
								padz(newTokens, capacity * sizeof(char*));
								memcpy(newTokens, tokens, token_count * sizeof(char*));
								ffree(tokens);
								tokens = newTokens;
							}
							tokens[token_count] = fmalloc(buf_index + 1);
							memcpy(tokens[token_count], tokenBuffer, buf_index + 1);
							token_count++;
							buf_index = 0;
						}
					} else {
						if (buf_index < sizeof(tokenBuffer) - 1) {
							tokenBuffer[buf_index++] = c;
						}
					}
					break;
				case 1:
					if (c == '"') {
						state = 0;
					} else if (c == '\\') {
						esc = 1;
					} else {
						if (buf_index < sizeof(tokenBuffer) - 1) {
							tokenBuffer[buf_index++] = c;
						}
					}
					break;
				case 2:
					if (c == '\'') {
						state = 0;
					} else if (c == '\\') {
						esc = 1;
					} else {
						if (buf_index < sizeof(tokenBuffer) - 1) {
							tokenBuffer[buf_index++] = c;
						}
					}
				default:
					break;
			}
		}
	}

	if (buf_index > 0) {
		tokenBuffer[buf_index] = '\0';
		if(token_count >= capacity) {
			capacity++;
			char **newTokens = fmalloc(capacity * sizeof(char*));
			padz(newTokens, capacity * sizeof(char*));
			memcpy(newTokens, tokens, token_count * sizeof(char*));
			ffree(tokens);
			tokens = newTokens;
		}
		tokens[token_count] = fmalloc(buf_index + 1);
		memcpy(tokens[token_count], tokenBuffer, buf_index + 1);
		token_count++;
	}

	if(token_count >= capacity) {
		capacity++;
		char **newTokens = fmalloc(capacity * sizeof(char*));
		padz(newTokens, capacity * sizeof(char*));
		memcpy(newTokens, tokens, token_count * sizeof(char*));
		ffree(tokens);
		tokens = newTokens;
	}
	tokens[token_count] = NULL;

	return tokens;
}

static void freeCmdStruct(char **cmdStruct) {
	for(int i = 0; cmdStruct[i]; i++) {
		ffree(cmdStruct[i]);
	}
	ffree(cmdStruct);
}

static void draw_prompt() {
	if(!getenv("PS1") || getenv("PS1")[0] == '\0') {
		if(getuid() == 0) {
			sprint("# ");
		} else {
			sprint("$ ");
		}
	} else {
		sprint(getenv("PS1"));
	}
}

static void __sigint_callback(int sig) {
	(void)sig;

	sprint("\n");
	draw_prompt();
}

int simpsh_main(int argc, char *argv[]) {
	int retValue = 0;
	char cmdBuf[2048];
	int inputFd = 0;
	signal(SIGINT, __sigint_callback);

	while(1) {
		padz(cmdBuf, sizeof(cmdBuf));

		// Like ./simpsh <<< cmd
		if (isatty(inputFd)) {
			draw_prompt();
		}

		size_t readsize = read(inputFd, cmdBuf, sizeof(cmdBuf));
		if (readsize <= 1) {
			continue;
		}
		cmdBuf[readsize - 1] = '\0';


		// Like:
		//  echo a; echo b
		// or:
		//  echo a
		//  echo b

		int lineCount = 0;
		char **pipeCommands;
		int pipeCount = 0;

		char **lines = parse_command(cmdBuf, ";\n");
		if (!lines[0])
			continue;

lineExec:
		// Parse pipe commands
		pipeCommands = parse_command(lines[lineCount], "|");
		pipeCount = 0;
		for(pipeCount = 0; pipeCommands[pipeCount]; pipeCount++);
		
		if (pipeCount > 1) {
			// Create pipes
			int *pipefds = fmalloc((pipeCount - 1) * 2 * sizeof(int));
			for (int i = 0; i < pipeCount - 1; i++) {
				if (pipe(pipefds + i * 2) == -1) {
					eprint("pipe");
					ffree(pipefds);
					ffree(lines);
					freeCmdStruct(pipeCommands);
					continue;
				}
			}

			// Create processes for each command
			pid_t *pids = fmalloc(pipeCount * sizeof(pid_t));
			for (int i = 0; i < pipeCount; i++) {
				pids[i] = fork();
				if (pids[i] < 0) {
					eprint("fork");
					continue;
				}

				if (pids[i] == 0) {
					// Set up input redirection
					if (i > 0) {
						dup2(pipefds[(i-1)*2], STDIN_FILENO);
					}

					// Set up output redirection
					if (i < pipeCount - 1) {
						dup2(pipefds[i*2 + 1], STDOUT_FILENO);
					}

					// Close all pipe ends
					for (int j = 0; j < pipeCount - 1; j++) {
						close(pipefds[j*2]);
						close(pipefds[j*2 + 1]);
					}

					// Execute command
					char **cmdArgs = parse_command(pipeCommands[i], " ");
					execvp(cmdArgs[0], cmdArgs);
					eprint("execvp");
					_exit(1);
				}
			}

			// Parent closes all pipe ends
			for (int i = 0; i < pipeCount - 1; i++) {
				close(pipefds[i*2]);
				close(pipefds[i*2 + 1]);
			}

			// Wait for all child processes
			for (int i = 0; i < pipeCount; i++) {
				if (pids[i] > 0) {
					waitpid(pids[i], NULL, 0);
				}
			}

			ffree(pipefds);
			ffree(pids);
			ffree(lines);
			freeCmdStruct(pipeCommands);
			continue;
		}
		
		freeCmdStruct(pipeCommands);

		char **cmdStruct = parse_command(lines[lineCount], " ");

		// Built-in commands
		if (cmdStruct[0]) {
			if (scmp(cmdStruct[0], "exit")) {
				sprint("exit\n");
				if(cmdStruct[1])
					retValue = atoi(cmdStruct[1]);
				freeCmdStruct(cmdStruct);
				freeCmdStruct(lines);
				break;
			}
			else if (scmp(cmdStruct[0], "cd")) {
				if(!cmdStruct[1]) {
					chdir(getenv("HOME") ? getenv("HOME") : "/");
				} else {
					if(chdir(cmdStruct[1])) {
						eprint("spsh");
					}
				}
				goto skipThis;
			}
			else if (scmp(cmdStruct[0], "unset")) {
				if(!cmdStruct[1]) {
					fprintf(stderr, "unset: no enough arguments\n");
					retValue = 1;
					goto skipThis;
				} else {
					int unsetCount = 1;
					for(; cmdStruct[unsetCount]; unsetCount++) {
						unsetenv(cmdStruct[unsetCount]);
					}
					goto skipThis;
				}
			}
		}

		if (!cmdStruct[0]) {
skipThis:
			freeCmdStruct(lines);
			freeCmdStruct(cmdStruct);
			continue;
		}

		size_t varNameGroupCount = 0;
		size_t cmd_start = 0;
		int shouldUnsetLast = 0;
		char *varNameGroup[1024];
		char *varName;
checkAgain:
		if (cmdStruct[cmd_start]
			&& isEquation(cmdStruct[cmd_start])
			) {
			char *eq = strchr(cmdStruct[cmd_start], '=');
			varName = strdup(cmdStruct[cmd_start]);
			if (varName) {
				varNameGroup[varNameGroupCount++] = varName;

				varName[eq - cmdStruct[cmd_start]] = '\0';
				setenv(varName, ++eq, 1);

				// Clean up
				cmd_start ++;

				// Check again
				goto checkAgain;
			}
		} else if (cmdStruct[cmd_start] && cmd_start != 0) {	// Not NULL and equation, like 'a=b env'
			shouldUnsetLast = 1;
		}

		if (!cmdStruct[cmd_start]) {
			for (size_t i = 0; i < varNameGroupCount; i++) {
				ffree(varNameGroup[i]);
			}
			varNameGroupCount = 0;
			goto skipThis;
		}

		pid_t spid = fork();
		if(spid < 0) {
			eprint("spsh: fork failed");
		} else if (spid == 0) {
			execvp(cmdStruct[cmd_start], &cmdStruct[cmd_start]);
			eprint(cmdStruct[cmd_start]);
			exit(1);
		} else {
			wait(&retValue);
			retValue /= 256;

			// like 'a=b env', we should unset this enviroment
			for (size_t i = 0; i < varNameGroupCount; i++) {
				if (shouldUnsetLast) {
					unsetenv(varNameGroup[i]);
				}
				ffree(varNameGroup[i]); // Make sure that all memory are freed
			}
			varNameGroupCount = 0;
			shouldUnsetLast = 0;
		}

		freeCmdStruct(cmdStruct);

		if (lines[++lineCount]) {
			goto lineExec;
		} else {
			freeCmdStruct(lines);
		}
		if (!isatty(inputFd)) {
			break;
		}
	}

	return retValue;
}

REGISTER_MODULE(simpsh);
