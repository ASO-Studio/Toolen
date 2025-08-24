/*
 * getch.c - Read a character from stdin
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#include "lib.h"

static struct termios original_termios;

static void enable_raw_mode() {
	struct termios raw;
	tcgetattr(STDIN_FILENO, &original_termios);
	raw = original_termios;
	raw.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

static int read_byte(int timeout_ms) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	struct timeval tv = {
		.tv_sec = timeout_ms / 1000,
		.tv_usec = (timeout_ms % 1000) * 1000
	};

	if (select(STDIN_FILENO + 1, &fds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL) <= 0)
		return -1;

	unsigned char c;
	if (read(STDIN_FILENO, &c, 1) != 1)
		return -1;
	return c;
}

int getch() {
	enable_raw_mode();

	int ch = read_byte(-1);  // Read the first character

	if (ch == KEY_ESC) {
		// ANSI
		int seq[2] = {-1, -1};
		for (int i = 0; i < 2; i++) {
			int ret = read_byte(20);
			if (ret == -1) break;
			seq[i] = ret;
		}

		// Up/Down/Left/Right
		if (seq[0] == '[' && seq[1] >= 'A' && seq[1] <= 'H') {
			switch (seq[1]) {
				case 'A': ch = KEY_UP;	break;
				case 'B': ch = KEY_DOWN;  break;
				case 'C': ch = KEY_RIGHT; break;
				case 'D': ch = KEY_LEFT;  break;
				case 'H': ch = KEY_HOME; break;
				case 'F': ch = KEY_END; break;
			}
		}
		// Alt + x
		else if (seq[0] >= 'a' && seq[0] <= 'z') {
			ch = KEY_ALT(seq[0]);
		}
		else if (seq[0] == 'O' && seq[1] >= 'P' && seq[1] <= 'S') {
			switch (seq[1]) {
				case 'P': ch = KEY_F(1); break;
				case 'Q': ch = KEY_F(2); break;
				case 'R': ch = KEY_F(3); break;
				case 'S': ch = KEY_F(4); break;
			}
		}
		else if (seq[0] == '[' && seq[1] == '1' || seq[1] == '2') {
			int seq3 = read_byte(20);
			if( seq3 == -1 ) {
				ch = KEY_ESC;
				goto end;
			}
			if(seq[1] == '1') {
				switch(seq3) {
					case '5': ch = KEY_F(5); break;
					/* \033[16~ -> ? */
					case '7': ch = KEY_F(6); break;
					case '8': ch = KEY_F(7); break;
					case '9': ch = KEY_F(8); break;
				}
			} else if (seq[1] == '2') {
				switch(seq3) {
					case '0': ch = KEY_F(9); break;
					case '1': ch = KEY_F(10); break;
					case '3': ch = KEY_F(11); break;
					case '4': ch = KEY_F(12); break;
				}
			}
		}
		// Other => ESC
		else ch = KEY_ESC;
	}
end:
	disable_raw_mode();
	return ch;
}
