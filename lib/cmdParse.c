#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lib.h"

static void padz(void* m, size_t len) {
	for(size_t i=0; i<len; i++) {
		UNSIG(m)[i] = 0;
	}
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
char** parse_command(const char *input, const char *delimiters) {
	char **tokens = xmalloc(32 * sizeof(char*));
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
								char **newTokens = xmalloc(capacity * sizeof(char*));
								padz(newTokens, capacity * sizeof(char*));
								memcpy(newTokens, tokens, token_count * sizeof(char*));
								free(tokens);
								tokens = newTokens;
							}
							tokens[token_count] = xmalloc(buf_index + 1);
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
			char **newTokens = xmalloc(capacity * sizeof(char*));
			padz(newTokens, capacity * sizeof(char*));
			memcpy(newTokens, tokens, token_count * sizeof(char*));
			free(tokens);
			tokens = newTokens;
		}
		tokens[token_count] = xmalloc(buf_index + 1);
		memcpy(tokens[token_count], tokenBuffer, buf_index + 1);
		token_count++;
	}

	if(token_count >= capacity) {
		capacity++;
		char **newTokens = xmalloc(capacity * sizeof(char*));
		padz(newTokens, capacity * sizeof(char*));
		memcpy(newTokens, tokens, token_count * sizeof(char*));
		free(tokens);
		tokens = newTokens;
	}
	tokens[token_count] = NULL;

	return tokens;
}
