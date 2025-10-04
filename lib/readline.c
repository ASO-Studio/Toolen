/*
 * readline.c - Line editing library with history support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lib.h"

#define INITIAL_SIZE 1024
#define MAX_HISTORY 100

// ANSI escape codes for terminal control
#define CLEAR_LINE "\033[2K\r"
#define CURSOR_LEFT(n) "\033[" #n "D"
#define CURSOR_RIGHT(n) "\033[" #n "C"

// History structure
static char *history[MAX_HISTORY];
static int history_count = 0;
static int history_index = -1;

static char prompt[1024];

// Insert a character at specified position
static size_t insert_char(int ch, char *str, size_t pos, size_t len, size_t *size) {
    // Check if we need to expand buffer
    if (len + 1 >= *size) {
        *size *= 2;
        str = xrealloc(str, *size);
    }
    
    // Shift characters to make space
    if (pos < len) {
        memmove(str + pos + 1, str + pos, len - pos);
    }
    
    // Insert character
    str[pos] = ch;
    str[len + 1] = '\0';
    
    return len + 1;
}

//Delete character at specified position
static size_t delete_char(char *str, size_t pos, size_t len) {
    if (len == 0 || pos >= len) return len;
    
    // Shift characters to fill the gap
    if (pos < len - 1) {
        memmove(str + pos, str + pos + 1, len - pos);
    }
    
    str[len - 1] = '\0';
    return len - 1;
}

// Refresh the prompt display
static void flush_prompt(size_t pos, const char *str) {
    // Clear current line and return to start
    printf(CLEAR_LINE);
    
    // Print prompt and current string
    printf("%s%s", prompt, str);
    fflush(stdout);
    
    // Calculate cursor position relative to prompt
    size_t total_len = strlen(prompt) + strlen(str);
    size_t target_pos = strlen(prompt) + pos;
    
    // Move cursor to correct position
    if (target_pos < total_len) {
        size_t move_left = total_len - target_pos;
        printf("\033[%zuD", move_left);
        fflush(stdout);
    }
}

// add_to_history - Add a string to history
static void add_to_history(const char *str) {
    if (history_count >= MAX_HISTORY) {
        // Remove oldest entry
        xfree(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        history_count--;
    }
    
    history[history_count++] = xstrdup(str);
    history_index = history_count; // Point to "new line" position
}

// Get history entry by index
static const char *get_history(int index) {
    if (index < 0 || index >= history_count) return NULL;
    return history[index];
}

// Main function
char *xreadline(const char *pmpt) {
    strncpy(prompt, pmpt ? pmpt : "> ", sizeof(prompt) - 1);
    prompt[sizeof(prompt) - 1] = '\0';
    
    size_t size = INITIAL_SIZE;
    char *str = xmalloc(size);
    size_t len = 0;
    size_t pos = 0;
    int ch;
    
    str[0] = '\0';
    flush_prompt(pos, str);
    
    while ((ch = getch()) != EOF) {
        if (ch == '\n') {
            putchar('\n');
            break;
        }
        
        switch(ch) {
            case KEY_UP:    // History up
                if (history_index > 0) {
                    history_index--;
                    const char *hist = get_history(history_index);
                    if (hist) {
                        len = strlen(hist);
                        if (len >= size) {
                            size = len + 1;
                            str = xrealloc(str, size);
                        }
                        strcpy(str, hist);
                        pos = len;
                        flush_prompt(pos, str);
                    }
                }
                break;
                
            case KEY_DOWN:  // History down
                if (history_index < history_count - 1) {
                    history_index++;
                    const char *hist = get_history(history_index);
                    if (hist) {
                        len = strlen(hist);
                        if (len >= size) {
                            size = len + 1;
                            str = xrealloc(str, size);
                        }
                        strcpy(str, hist);
                        pos = len;
                    } else {
                        // Current line (not in history yet)
                        str[0] = '\0';
                        len = 0;
                        pos = 0;
                    }
                    flush_prompt(pos, str);
                }
                break;
                
            case KEY_LEFT:  // Move cursor left
                if (pos > 0) {
                    pos--;
                    flush_prompt(pos, str);
                }
                break;
                
            case KEY_RIGHT: // Move cursor right
                if (pos < len) {
                    pos++;
                    flush_prompt(pos, str);
                }
                break;
                
            case KEY_HOME:  // Move to start of line
                pos = 0;
                flush_prompt(pos, str);
                break;
                
            case KEY_END:   // Move to end of line
                pos = len;
                flush_prompt(pos, str);
                break;
                
            case '\b':      // Backspace
            case 127:       // Delete key (often sent as 127)
                if (pos > 0) {
                    len = delete_char(str, pos - 1, len);
                    pos--;
                    flush_prompt(pos, str);
                }
                break;
                
            default:        // Printable characters
                if (isprint(ch)) {
                    len = insert_char(ch, str, pos, len, &size);
                    pos++;
                    flush_prompt(pos, str);
                }
                break;
        }
    }
    
    // Add non-empty lines to history
    if (len > 0) {
        add_to_history(str);
    }
    
    return str;
}
