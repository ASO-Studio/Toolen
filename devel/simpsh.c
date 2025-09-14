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
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include "config.h"
#include "module.h"
#include "lib.h"

static void sprint(const char *str) {
	write(1, str, strlen(str));
}

static void eprint(const char *str) {
	fprintf(stderr, "%s: %s\n", str, strerror(errno));
}

static fused void itoa(unsigned int num,char *buffer){
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

static void freeCmdStruct(char **cmdStruct) {
	for(int i = 0; cmdStruct[i]; i++) {
		xfree(cmdStruct[i]);
	}
	xfree(cmdStruct);
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

// Remove comments from string
void remove_comments(char* str) {
	if (!str) return;

	char* write_ptr = str;
	int in_1quotes = 0;
	int in_2quotes = 0;

	for (char* p = str; *p; p++) {
		if (*p == '"' && ! !in_1quotes) {
			in_2quotes = !in_2quotes;
			*write_ptr++ = *p;
		} else if (*p == '\'' && !in_2quotes) {
			in_1quotes = !in_1quotes;
			*write_ptr++ = *p;
		} else if (*p == '#' && !in_1quotes && !in_2quotes)
			break;
		else
			*write_ptr++ = *p;
	}

	while (write_ptr > str) {
		if (isspace(*(write_ptr-1)))
			write_ptr--;
		else
			break;
	}
	*write_ptr = '\0';
}

static void simpsh_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: simpsh [FILE]\n\n"
			"A simple shell program\n");
}

M_ENTRY(simpsh) {
	if (findArg(argv, argc, "--help")) {
		simpsh_show_help();
		return 0;
	}

	const char *inputFile = argv[1];
	int retValue = 0;
	char cmdBuf[2048];
	FILE *inputFp = NULL;
	int inputFd = 0;

	if (!inputFile) {
		signal(SIGINT, __sigint_callback);
	} else {
		inputFp = xfopen(inputFile, "r");
	}

	while(1) {
		padz(cmdBuf, sizeof(cmdBuf));

		// Like ./simpsh <<< cmd
		if (isatty(inputFd) && !inputFile) {
			draw_prompt();
		}

		if (!inputFile) {
			size_t readsize = read(inputFd, cmdBuf, sizeof(cmdBuf));
			if (readsize <= 1) {
				continue;
			}
			cmdBuf[readsize - 1] = '\0';
		} else {
			if (!fgets(cmdBuf, sizeof(cmdBuf), inputFp))
				break;
		}

		// Remove all comments in sentence
		remove_comments(cmdBuf);

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
			int *pipefds = xmalloc((pipeCount - 1) * 2 * sizeof(int));
			for (int i = 0; i < pipeCount - 1; i++) {
				if (pipe(pipefds + i * 2) == -1) {
					eprint("pipe");
					xfree(pipefds);
					xfree(lines);
					freeCmdStruct(pipeCommands);
					continue;
				}
			}

			// Create processes for each command
			pid_t *pids = xmalloc(pipeCount * sizeof(pid_t));
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
					char **cmdArgs = parse_command(pipeCommands[i], " \t");
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

			xfree(pipefds);
			xfree(pids);
			xfree(lines);
			freeCmdStruct(pipeCommands);
			continue;
		}
		
		freeCmdStruct(pipeCommands);

		char **cmdStruct = parse_command(lines[lineCount], " \t");

		// Built-in commands
		if (cmdStruct[0]) {
			if (scmp(cmdStruct[0], "exit")) {
				if (!inputFile)
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
				xfree(varNameGroup[i]);
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
				xfree(varNameGroup[i]); // Make sure that all memory are freed
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
