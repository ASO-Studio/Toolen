/*
 * execInPipe.c - Execute command and redirect output to pipe
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define INITIAL_BUF_SIZE 1024

char *execInPipe(const char *command, char *const *argv) {
	int pipefd[2];
	pid_t pid;
	char *buffer = NULL;
	size_t buf_size = INITIAL_BUF_SIZE;
	size_t total_read = 0;
	ssize_t nread;

	// Create pipe
	if (pipe(pipefd) == -1) {
		perror("pipe");
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		perror("fork");
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	}

	if (pid == 0) {
		close(pipefd[0]);

		if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}
		close(pipefd[1]);

		execvp(command, argv);

		perror("execvp");
		exit(EXIT_FAILURE);
	} else {
		close(pipefd[1]);

		// Allocate memory
		buffer = malloc(buf_size);
		if (!buffer) {
			perror("malloc");
			close(pipefd[0]);
			return NULL;
		}

		while (1) {
			if (total_read >= buf_size - 1) {
				buf_size *= 2;
				char *new_buf = realloc(buffer, buf_size);
				if (!new_buf) {
					perror("realloc");
					free(buffer);
					close(pipefd[0]);
					return NULL;
				}
				buffer = new_buf;
			}

			// Read command output from pipe
			nread = read(pipefd[0], buffer + total_read, buf_size - total_read - 1);
			if (nread == -1) {
				if (errno == EINTR) continue;
				perror("read");
				free(buffer);
				close(pipefd[0]);
				return NULL;
			}
			if (nread == 0) break;	// EOF
			total_read += nread;
		}
		close(pipefd[0]);

		int status;
		waitpid(pid, &status, 0);
		buffer[total_read] = '\0';

		char *final_buf = realloc(buffer, total_read + 1);
		if (final_buf) {
			return final_buf;
		}
		return buffer;
	}
}
