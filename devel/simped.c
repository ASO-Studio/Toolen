/**
 *	simpsh.c - Simple editor
 *
 * 	Created by RoofAlan
 *		2025/8/27
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "module.h"
#include "lib.h"

struct termios original_termios;

typedef int POSITION;

// Text line structure
typedef struct Line {
	char *text;
	size_t length;
	struct Line *next;
	struct Line *prev;
} Line;

typedef struct {
	int ffd;		// File FD
	POSITION x, y;		// Cursor position
	int size_x, size_y;	// Terminal size
	enum {
		E_CMD,		// Command line mode
		E_EDI,		// Editor mode
	} mode;

	enum {
		FS_NEW,		// New file
		FS_EST,		// Exist file
	} fileStat;

	const char *file;	// File path

	// Text buffer
	Line *lines;
	Line *current_line;
	int line_count;
	int scroll_offset;	// Scroll offset
	int modified;		// Does file changed
} edStat;

// Get terimal size
static int getTermSize(int *x, int *y) {
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		return -1;
	}

	*x = ws.ws_col;
	*y = ws.ws_row;
	return 0;
}

// Set terminal to raw mode
static void set_terminal_raw() {
	struct termios raw = original_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// Show/Hide cursor
static void show_cursor(int show) {
	if (show) {
		printf("\033[?25h"); // Show
	} else {
		printf("\033[?25l"); // Hide
	}
	fflush(stdout);
}

// Initialize a line
static Line* init_line() {
	Line *line = malloc(sizeof(Line));
	line->text = malloc(1);
	line->text[0] = '\0';
	line->length = 0;
	line->next = NULL;
	line->prev = NULL;
	return line;
}

// Insert charactor in line
static void insert_char(Line *line, int pos, char c) {
	if (pos < 0 || pos > (int)line->length) return;
	
	line->text = realloc(line->text, line->length + 2);
	memmove(line->text + pos + 1, line->text + pos, line->length - pos + 1);
	line->text[pos] = c;
	line->length++;
}

// Insert string in line
static void insert_string(Line *line, int pos, const char *str) {
	if (pos < 0 || pos > (int)line->length) return;
	
	size_t len = strlen(str);
	line->text = realloc(line->text, line->length + len + 1);
	memmove(line->text + pos + len, line->text + pos, line->length - pos + 1);
	memcpy(line->text + pos, str, len);
	line->length += len;
}

// Delete charactor in line
static void delete_char(Line *line, int pos) {
	if (pos < 0 || pos >= (int)line->length) return;
	
	memmove(line->text + pos, line->text + pos + 1, line->length - pos);
	line->length--;
	line->text = realloc(line->text, line->length + 1);
}

// Split a line
static void splitLine(edStat *ed, Line *line, int pos) {
	Line *new_line = init_line();
	
	if (pos < (int)line->length) {
		new_line->length = line->length - pos;
		new_line->text = realloc(new_line->text, new_line->length + 1);
		memcpy(new_line->text, line->text + pos, new_line->length);
		new_line->text[new_line->length] = '\0';
	}
	
	line->length = pos;
	line->text = realloc(line->text, line->length + 1);
	line->text[line->length] = '\0';
	
	new_line->next = line->next;
	new_line->prev = line;
	if (line->next) line->next->prev = new_line;
	line->next = new_line;
	
	ed->line_count++;
	ed->modified = 1;
}

// Merge 2 lines
static void mergeLines(edStat *ed, Line *line) {
	if (!line || !line->next) return;
	
	Line *next_line = line->next;
	size_t new_length = line->length + next_line->length;
	
	line->text = realloc(line->text, new_length + 1);
	memcpy(line->text + line->length, next_line->text, next_line->length);
	line->length = new_length;
	line->text[line->length] = '\0';
	
	line->next = next_line->next;
	if (next_line->next) next_line->next->prev = line;
	
	free(next_line->text);
	free(next_line);
	
	ed->line_count--;
	ed->modified = 1;
}

// Load a file
static int loadFile(edStat *ed) {
	struct stat st;
	if (fstat(ed->ffd, &st) == -1) return 1;
	
	if (st.st_size == 0) {
		ed->lines = init_line();
		ed->current_line = ed->lines;
		ed->line_count = 1;
		return 0;
	}
	
	char *content = malloc(st.st_size + 1);
	if (read(ed->ffd, content, st.st_size) != st.st_size) {
		free(content);
		return 1;
	}
	content[st.st_size] = '\0';
	
	ed->lines = init_line();
	Line *current = ed->lines;
	char *start = content;
	char *end;
	
	ed->line_count = 0;
	
	while ((end = strchr(start, '\n')) != NULL) {
		size_t length = end - start;
		current->text = realloc(current->text, length + 1);
		memcpy(current->text, start, length);
		current->text[length] = '\0';
		current->length = length;
		
		current->next = init_line();
		current->next->prev = current;
		current = current->next;
		
		start = end + 1;
		ed->line_count++;
	}
	
	// Process the last line
	if (*start != '\0') {
		size_t length = strlen(start);
		current->text = realloc(current->text, length + 1);
		memcpy(current->text, start, length);
		current->text[length] = '\0';
		current->length = length;
		ed->line_count++;
	}
	
	free(content);
	ed->current_line = ed->lines;
	return 0;
}

// Save the file content
static int save_file(edStat *ed) {
	if (lseek(ed->ffd, 0, SEEK_SET) == -1) return 1;
	if (ftruncate(ed->ffd, 0) == -1) return 1;
	
	Line *current = ed->lines;
	while (current) {
		if (write(ed->ffd, current->text, current->length) != (ssize_t)current->length) return 1;
		if (current->next && write(ed->ffd, "\n", 1) != 1) return 1;
		current = current->next;
	}
	
	ed->modified = 0;
	return 0;
}

// Free all lines
static void free_lines(Line *line) {
	while (line) {
		Line *next = line->next;
		free(line->text);
		free(line);
		line = next;
	}
}

// Initialize the editor
static int initEditor(edStat *ed, const char *file) {
	ed->x = 0;
	ed->y = 0;
	if (getTermSize(&(ed->size_x), &(ed->size_y)) < 0) {
		return 1;
	}

	ed->fileStat = (access(file, F_OK) == 0) ? FS_EST : FS_NEW;
	ed->file = file;
	ed->ffd = open(file, O_RDWR | O_CREAT, 0666);
	if (ed->ffd < 0) {
		return 1;
	}

	ed->mode = E_CMD;
	ed->scroll_offset = 0;
	ed->modified = 0;
	
	if (loadFile(ed) != 0) {
		close(ed->ffd);
		return 1;
	}

	return 0;
}

// Close editor
static void closeEditor(edStat *ed) {
	free_lines(ed->lines);
	close(ed->ffd);
}

// Move the cursor to the start of line
static void move_to_line_start(edStat *ed) {
	ed->x = 0;
}

// Move the cursor to the end of line
static void move_to_line_end(edStat *ed) {
	ed->x = ed->current_line->length;
}

// Move cursor
static void move_cursor(edStat *ed, int dx, int dy) {
	if (dx != 0) {
		ed->x += dx;
		if (ed->x < 0) ed->x = 0;
		if (ed->x > (int)ed->current_line->length) ed->x = ed->current_line->length;
	}

	if (dy != 0) {
		if (dy < 0 && ed->current_line->prev) {
			ed->current_line = ed->current_line->prev;
			ed->y--;

			if (ed->x > (int)ed->current_line->length)
				ed->x = ed->current_line->length;
		}
		else if (dy > 0 && ed->current_line->next) {
			ed->current_line = ed->current_line->next;
			ed->y++;

			if (ed->x > (int)ed->current_line->length)
				ed->x = ed->current_line->length;
		}
	}
}

// Clear screen
static void clearScreen() {
	printf("\033[H\033[2J\033[3J");
}

// Show the status bar
static void show_status_bar(edStat *ed) {
	printf("\033[%d;1H\033[7m", ed->size_y - 1);
	printf("%s - %s - %d,%d - %d lines %s", 
		   ed->file, 
		   ed->mode == E_CMD ? "COMMAND" : "INSERT", 
		   ed->y + 1, ed->x + 1,
		   ed->line_count,
		   ed->modified ? "(Modified)" : "");
	int status_len = strlen(ed->file) + 15 + 10 + (ed->modified ? 11 : 0);
	for (int i = status_len; i < ed->size_x; i++) {
		putchar(' ');
	}
	printf("\033[0m"); // Reset the attribute
}

// Display the content
static void displayText(edStat *ed) {
	clearScreen();

	Line *line = ed->lines;
	for (int i = 0; i < ed->scroll_offset && line; i++) {
		line = line->next;
	}

	for (int i = 0; i < ed->size_y - 1 && line; i++) {
		printf("\033[%d;1H", i + 1);
		printf("%.*s", ed->size_x, line->text);
		line = line->next;
	}

	// Move the cursor to proper position
	int display_y = ed->y - ed->scroll_offset + 1;
	if (display_y > 0 && display_y < ed->size_y) {
		printf("\033[%d;%dH", display_y, ed->x + 1);
	}

	show_status_bar(ed);

	if (display_y > 0 && display_y < ed->size_y) {
		printf("\033[%d;%dH", display_y, ed->x + 1);
	}

	show_cursor(1);
	fflush(stdout);
}

// Command mode
static int commandMode(edStat *ed, int key) {
	switch (key) {
		case 'i':
			ed->mode = E_EDI;
			break;
		case ':':
			printf("\033[%d;1H:", ed->size_y);
			fflush(stdout);
			
			char command[100];
			int pos = 0;
			while (1) {
				int c = getch();

				if (c == KEY_ESC) {
					goto endCommandMode;	// Exit the command mode
				}

				if (c == KEY_ENTER) {
					command[pos] = '\0';
					break;
				} else if (c == KEY_BKSPE && pos > 0) {
					pos--;
					printf("\b \b");
					fflush(stdout);
				} else if (isprint(c) && (size_t)pos < sizeof(command) - 1) {
					command[pos++] = c;
					putchar(c);
					fflush(stdout);
				}
			}
			
			// Process
			if (strcmp(command, "w") == 0) {
				if (save_file(ed) == 0) {
					printf("\033[%d;1H\033[2KFile saved", ed->size_y);
				} else {
					printf("\033[%d;1H\033[2KError saving file", ed->size_y);
				}
			} else if (strcmp(command, "q") == 0) {
				if (ed->modified) {
					printf("\033[%d;1H\033[2KFile modified. Use :wq to save and quit", ed->size_y);
				} else {
					if (ed->fileStat == FS_NEW)
						remove(ed->file);
					return 1;
				}
			} else if (strcmp(command, "wq") == 0) {
				if (save_file(ed) == 0) {
					return 1;
				} else {
					printf("\033[%d;1H\033[2KError saving file", ed->size_y);
				}
			} else if (strcmp(command, "q!") == 0) {
				close(ed->ffd);

				if (ed->fileStat == FS_NEW)	// If it is a new file, delete it
					remove(ed->file);

				return 1;
			} else {
				printf("\033[%d;1H\033[2K"
					"Unknown command: %s", ed->size_y, command);
			}
endCommandMode:
			break;
		case KEY_UP:
			move_cursor(ed, 0, -1);
			break;
		case KEY_DOWN:
			move_cursor(ed, 0, 1);
			break;
		case KEY_LEFT:
			move_cursor(ed, -1, 0);
			break;
		case KEY_RIGHT:
			move_cursor(ed, 1, 0);
			break;
		case KEY_HOME:
			move_to_line_start(ed);
			break;
		case KEY_END:
			move_to_line_end(ed);
			break;
	}
	return 0;
}

// Process edit mode
static void editMode(edStat *ed, int key) {
	switch (key) {
		case KEY_ESC:
			ed->mode = E_CMD;
			break;
		case KEY_UP:
			move_cursor(ed, 0, -1);
			break;
		case KEY_DOWN:
			move_cursor(ed, 0, 1);
			break;
		case KEY_LEFT:
			move_cursor(ed, -1, 0);
			break;
		case KEY_RIGHT:
			move_cursor(ed, 1, 0);
			break;
		case KEY_BKSPE:
			if (ed->x > 0) {
				delete_char(ed->current_line, ed->x - 1);
				ed->x--;
				ed->modified = 1;
			} else if (ed->current_line->prev) {
				// Merge lines
				int prev_length = ed->current_line->prev->length;
				mergeLines(ed, ed->current_line->prev);
				ed->current_line = ed->current_line->prev;
				ed->y--;
				ed->x = prev_length;
			}
			break;
		case KEY_ENTER:
			splitLine(ed, ed->current_line, ed->x);
			ed->current_line = ed->current_line->next;
			ed->y++;
			ed->x = 0;
			break;
		case KEY_TAB:
			// Insert 4 spaces for 'tab'
			insert_string(ed->current_line, ed->x, "    ");
			ed->x += 4;
			ed->modified = 1;
			break;
		case KEY_HOME:
			move_to_line_start(ed);
			break;
		case KEY_END:
			move_to_line_end(ed);
			break;
		default:
			if (isprint(key)) {
				insert_char(ed->current_line, ed->x, key);
				ed->x++;
				ed->modified = 1;
			}
			break;
	}
}

static int startEd() {
	if (tcgetattr(STDIN_FILENO, &original_termios) != 0) {
		return -1;
	}
	set_terminal_raw();
	show_cursor(1);
	return 0;
}

static void edMain(edStat *ed) {
	displayText(ed);
	
	while (1) {
		int key = getch();
		
		if (ed->mode == E_CMD) {
			int shouldExit = commandMode(ed, key);

			if (shouldExit) {	// Make sure that terminal could restore
				clearScreen();
				return;
			}

		} else {
			editMode(ed, key);
		}

		if (ed->y < ed->scroll_offset) {
			ed->scroll_offset = ed->y;
		} else if (ed->y >= ed->scroll_offset + ed->size_y - 1) {
			ed->scroll_offset = ed->y - ed->size_y + 2;
		}

		displayText(ed);
	}
}

static void endEd() {
	// Restore the terimal
	tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
	show_cursor(1);
}

static void simped_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: simped FILE\n\n"
			"Simple text editor\n\n"
			"Support command:\n"
			"  :q   Quit\n"
			"  :wq  Save the modified content to the file and quit\n"
			"  :w   Just Save the content\n"
			"  :q!  Quit and don't save the content\n");
}

M_ENTRY(simped) {
	if (argc < 2) {
		fprintf(stderr, "Usage: simped FILE\n"
						"Try pass '--help' for more details\n");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		simped_show_help();
		return 0;
	}

	edStat ed;
	if (initEditor(&ed, argv[1]) != 0) {
		perror("initEditor");
		return 1;
	}

	if (startEd() >= 0) {
		edMain(&ed);
		endEd();
	} else {
		perror("Failed to start editor");
		closeEditor(&ed);
		return 1;
	}

	closeEditor(&ed);
	return 0;
}

REGISTER_MODULE(simped);
