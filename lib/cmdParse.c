#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lib.h"

// Zero out a memory block
static void padz(void* m, size_t len) {
	for(size_t i = 0; i < len; i++) {
		UNSIG(m)[i] = 0;
	}
}

// Check if character is in delimiter set
static bool is_delimiter(char c, const char *delimiters) {
	for (size_t i = 0; delimiters[i] != '\0'; i++) {
		if (c == delimiters[i]) {
			return true;
		}
	}
	return false;
}

// Expand tokens array when capacity is reached
static void expand_tokens_array(char ***tokens, int *capacity, int token_count) {
	*capacity *= 2;
	char **newTokens = xmalloc(*capacity * sizeof(char*));
	padz(newTokens, *capacity * sizeof(char*));
	memcpy(newTokens, *tokens, token_count * sizeof(char*));
	xfree(*tokens);
	*tokens = newTokens;
}

// Parse command string into tokens array
// remove_quotes: 1 to remove quotes from tokens, 0 to keep quotes
char** parse_command(const char *input, const char *delimiters, int remove_quotes) {
	// Handle null inputs gracefully
	if (input == NULL || delimiters == NULL) {
		char **empty_tokens = xmalloc(sizeof(char*));
		empty_tokens[0] = NULL;
		return empty_tokens;
	}
	
	// Initialize tokens array and parsing buffer
	char **tokens = xmalloc(32 * sizeof(char*));
	char tokenBuffer[2048];
	int token_count = 0;
	int capacity = 32;
	int in_dquot = 0;  // Inside double quotes flag
	int in_squot = 0;  // Inside single quotes flag
	int esc = 0;	   // Escape character flag
	size_t buf_index = 0;
	size_t start = 0;

	// Initialize buffers
	padz(tokenBuffer, sizeof(tokenBuffer));
	padz(tokens, capacity * sizeof(char*));

	// Skip leading delimiters
	while (input[start] != '\0' && is_delimiter(input[start], delimiters)) {
		start++;
	}

	// Main parsing loop
	for (size_t i = start; input[i] != '\0'; i++) {
		char c = input[i];
		
		if (esc) {
			// Handle escape sequences
			if (buf_index < sizeof(tokenBuffer) - 1) {
				switch(c) {
					case 'n': tokenBuffer[buf_index++] = '\n'; break;
					case 't': tokenBuffer[buf_index++] = '\t'; break;
					case 'r': tokenBuffer[buf_index++] = '\r'; break;
					case '\\': tokenBuffer[buf_index++] = '\\'; break;
					case '"': tokenBuffer[buf_index++] = '"'; break;
					case '\'': tokenBuffer[buf_index++] = '\''; break;
					default: tokenBuffer[buf_index++] = c; break;
				}
			}
			esc = 0;
		} else {
			// Handle special characters
			if (c == '\\' && !in_squot) {
				esc = 1;  // Next character is escaped
			} else if (c == '"' && !in_squot) {
				// Toggle double quote state
				in_dquot = !in_dquot;
				// Add quote to token if requested
				if (!remove_quotes && buf_index < sizeof(tokenBuffer) - 1) {
					tokenBuffer[buf_index++] = c;
				}
			} else if (c == '\'' && !in_dquot) {
				// Toggle single quote state
				in_squot = !in_squot;
				// Add quote to token if requested
				if (!remove_quotes && buf_index < sizeof(tokenBuffer) - 1) {
					tokenBuffer[buf_index++] = c;
				}
			} else if (is_delimiter(c, delimiters) && !in_dquot && !in_squot) {
				// Finalize token when delimiter found outside quotes
				if (buf_index > 0) {
					tokenBuffer[buf_index] = '\0';
					if(token_count >= capacity) {
						expand_tokens_array(&tokens, &capacity, token_count);
					}
					tokens[token_count] = xmalloc(buf_index + 1);
					memcpy(tokens[token_count], tokenBuffer, buf_index + 1);
					token_count++;
					buf_index = 0;
				}
			} else {
				// Regular character, add to buffer
				if (buf_index < sizeof(tokenBuffer) - 1) {
					tokenBuffer[buf_index++] = c;
				}
			}
		}
	}

	// Handle trailing escape character
	if (esc && buf_index < sizeof(tokenBuffer) - 1) {
		tokenBuffer[buf_index++] = '\\';
	}

	// Finalize last token if buffer has content OR we're still inside quotes
	if (buf_index > 0 || in_dquot || in_squot) {
		tokenBuffer[buf_index] = '\0';
		if(token_count >= capacity) {
			expand_tokens_array(&tokens, &capacity, token_count);
		}
		tokens[token_count] = xmalloc(buf_index + 1);
		memcpy(tokens[token_count], tokenBuffer, buf_index + 1);
		token_count++;
	}

	// Ensure space for NULL terminator in tokens array
	if(token_count >= capacity) {
		expand_tokens_array(&tokens, &capacity, token_count);
	}
	tokens[token_count] = NULL;

	return tokens;
}
