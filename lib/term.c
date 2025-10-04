/*
 * term.c - Terminal operations
 */

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int getTerminalSize(int *col, int *row) {
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) { // Get terminal size
		return -1;
	}

	*col = ws.ws_col;
	*row = ws.ws_row;
	return 0;
}
