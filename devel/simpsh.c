#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "config.h"
#include "module.h"

#define UNSIG(x) ((unsigned char*)x)

static size_t strLength(const char *str) {
	size_t ret = 0;
	while(*str++) ret++;
	return ret;
}

static void sprint(const char *str) {
	write(1, str, strLength(str));
}

static void eprint(const char *str) {
	write(2, str, strLength(str));
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

	if(strLength(d) != strLength(s))
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

static void mcpy(void* d, const void* s, size_t len) {
	for(size_t i=0; i<len; i++) {
		UNSIG(d)[i] = UNSIG(s)[i];
	}
}

static void *fmalloc(size_t size) {
#if USE_FALLOC
	size_t rsize = size + sizeof(size_t);
	void *p = mmap(NULL, rsize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if(p == MAP_FAILED) {
		eprint("spsh: mmap failed\n");
		exit(1);
	}
	mcpy(p, &rsize, sizeof(size_t));
	return (void*)&(UNSIG(p)[sizeof(size_t)]);
#else
	void *p = malloc(size);
	if(!p) {
		eprint("spsh: mmap failed\n");
		exit(1);
	}
	return p;
#endif
}

static void ffree(void *ptr) {
#if USE_FALLOC
	void *rptr = (void*)UNSIG(ptr - sizeof(size_t));
	size_t rsize;
	mcpy(&rsize, rptr, sizeof(size_t));
	padz(rptr, rsize);
	munmap(rptr, rsize);
#else
	free(ptr);
#endif
}

static bool is_delimiter(char c, const char *delimiters) {
	for (size_t i = 0; delimiters[i] != '\0'; i++) {
		if (c == delimiters[i]) {
			return true;
		}
	}
	return false;
}

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
								mcpy(newTokens, tokens, token_count * sizeof(char*));
								ffree(tokens);
								tokens = newTokens;
							}
							tokens[token_count] = fmalloc(buf_index + 1);
							mcpy(tokens[token_count], tokenBuffer, buf_index + 1);
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
			mcpy(newTokens, tokens, token_count * sizeof(char*));
			ffree(tokens);
			tokens = newTokens;
		}
		tokens[token_count] = fmalloc(buf_index + 1);
		mcpy(tokens[token_count], tokenBuffer, buf_index + 1);
		token_count++;
	}

	if(token_count >= capacity) {
		capacity++;
		char **newTokens = fmalloc(capacity * sizeof(char*));
		padz(newTokens, capacity * sizeof(char*));
		mcpy(newTokens, tokens, token_count * sizeof(char*));
		ffree(tokens);
		tokens = newTokens;
	}
	tokens[token_count] = NULL;

	return tokens;
}

void freeCmdStruct(char **cmdStruct) {
	for(int i = 0; cmdStruct[i]; i++) {
		ffree(cmdStruct[i]);
	}
	ffree(cmdStruct);
}

int simpsh_main(int argc, char *argv[]) {
	int retValue = 0;
	char cmdBuf[2048];
	int inputFd = 0;

	while(1) {
		padz(cmdBuf, sizeof(cmdBuf));

		if(!getenv("PS1") || getenv("PS1")[0] == '\0') {
			if(getuid() == 0) {
				sprint("# ");
			} else {
				sprint("$ ");
			}
		} else {
			sprint(getenv("PS1"));
		}

		size_t readsize = read(inputFd, cmdBuf, sizeof(cmdBuf));
		if (readsize <= 1) {
			continue;
		}
		cmdBuf[readsize - 1] = '\0';

		char **cmdStruct = parse_command(cmdBuf, " ");

		if (cmdStruct[0]) {
			if (scmp(cmdStruct[0], "exit")) {
				sprint("exit\n");
				if(cmdStruct[1])
					retValue = atoi(cmdStruct[1]);
				for(int i = 0; cmdStruct[i]; i++) {
					ffree(cmdStruct[i]);
				}
				ffree(cmdStruct);
				break;
			}
			else if (scmp(cmdStruct[0], "cd")) {
				if(!cmdStruct[1]) {
					chdir(getenv("HOME") ? getenv("HOME") : "/");
				} else {
					if(chdir(cmdStruct[1])) {
						eprint("spsh: no such file or directory or permission denied\n");
					}
				}
				for(int i = 0; cmdStruct[i]; i++) {
					ffree(cmdStruct[i]);
				}
				ffree(cmdStruct);
				continue;
			}
		}

		if (!cmdStruct[0]) {
			freeCmdStruct(cmdStruct);
			continue;
		}

		pid_t spid = fork();
		if(spid < 0) {
			eprint("spsh: fork failed\n");
		} else if (spid == 0) {
			execvp(cmdStruct[0], cmdStruct);
			eprint("spsh: command not found or no such file or directory\n");
			exit(1);
		} else {
			wait(&retValue);
			retValue /= 256;
		}

		freeCmdStruct(cmdStruct);
		
	}

	return retValue;
}

REGISTER_MODULE(simpsh);
