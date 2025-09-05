/**
 *	kill.c - Kill process (SIGTERM by default)
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#include "module.h"
#include "config.h"

/* Maximum signal number to consider */
#ifdef _NSIG
# define MAX_SIGNAL _NSIG
#else
# define MAX_SIGNAL 32
#endif

static const char* __ssig(int sig) {
	switch(sig) {
		case SIGHUP: return "HUP";
		case SIGINT: return "INT";
		case SIGQUIT: return "QUIT";
		case SIGILL: return "ILL";
		case SIGTRAP: return "TRAP";
		case SIGABRT: return "ABRT";
		case SIGBUS: return "BUS";
		case SIGFPE: return "FPE";
		case SIGKILL: return "KILL";
		case SIGUSR1: return "USR1";
		case SIGSEGV: return "SEGV";
		case SIGUSR2: return "USR2";
		case SIGPIPE: return "PIPE";
		case SIGALRM: return "ALRM";
		case SIGTERM: return "TERM";
		case SIGSTKFLT: return "STKFLT";
		case SIGCHLD: return "CHLD";
		case SIGCONT: return "CONT";
		case SIGSTOP: return "STOP";
		case SIGTSTP: return "TSTP";
		case SIGTTIN: return "TTIN";
		case SIGTTOU: return "TTOU";
		case SIGURG: return "URG";
		case SIGXCPU: return "XCPU";
		case SIGXFSZ: return "XFSZ";
		case SIGVTALRM: return "VTALRM";
		case SIGPROF: return "PROF";
		case SIGWINCH: return "WINCH";
		case SIGPWR: return "PWR";
		case SIGSYS: return "SYS";
#ifdef SIGPOLL
		case SIGPOLL: return "POLL";
#endif
#ifdef SIGLOST
		case SIGLOST: return "LOST";
#endif
#ifdef SIGUNUSED
		case SIGUNUSED: return "UNUSED";
#endif
		default: break;
	}
	return "UNKWN";
}

/* Signal name conversion */
static int sig_name_to_num(const char *name) {
	if (!name) return -1;
	
	/* Try numeric format */
	if (isdigit(name[0])) {
		char *end;
		long num = strtol(name, &end, 10);
		if (*end == '\0' && num > 0 && num < MAX_SIGNAL) 
			return (int)num;
	}
	
	/* Try with SIG prefix */
	if (strncasecmp(name, "SIG", 3) == 0) {
		for (int i = 1; i < MAX_SIGNAL; i++) {
			const char *sig_name = __ssig(i);
			if (sig_name && strcasecmp(name + 3, sig_name) == 0)
				return i;
		}
	}
	
	/* Try without prefix */
	for (int i = 1; i < MAX_SIGNAL; i++) {
		const char *sig_name = strsignal(i);
		if (sig_name && strcasecmp(name, sig_name) == 0)
			return i;
	}
	
	return -1;
}

/* List available signals */
static void list_signals(void) {
	for (int i = 1; i < MAX_SIGNAL; i++) {
		const char *name = __ssig(i);
		if (name) {
			printf("%2d) SIG%-8s %s\n", i, name, name);
		}
	}
}

/* Show help information */
static void show_help(void) {
	SHOW_VERSION(stderr);
	fprintf(stderr,
		"Usage: kill [options] <pid>\n"
		"       kill [options] -<signal> <pid>\n"
		"       kill [options] --signal <signal> <pid>\n\n"
		"Send signal to process(es)\n\n"
		"Support options:\n"
		"  -l, --list     list all supported signals\n");
}

int kill_main(int argc, char *argv[]) {
	if (argc < 2) {
		show_help();
		return EXIT_FAILURE;
	}

	/* Handle options */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
			list_signals();
			return EXIT_SUCCESS;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			show_help();
			return EXIT_SUCCESS;
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
			SHOW_VERSION(stdout);
			return EXIT_SUCCESS;
		}
	}

	pid_t pid = 0;
	int sig = SIGTERM;  // Default signal

	/* Parse signal and PID arguments */
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* Handle signal specification */
			const char *sigspec = argv[i] + 1;
			
			/* Skip extra '-' for long options */
			if (sigspec[0] == '-') sigspec++;
			
			sig = sig_name_to_num(sigspec);
			if (sig < 0) {
				fprintf(stderr, "Invalid signal: %s\n", sigspec);
				fprintf(stderr, "Use 'kill --list' for available signals\n");
				return EXIT_FAILURE;
			}
		} 
		else {
			/* Handle PID */
			char *end;
			long val = strtol(argv[i], &end, 10);
			if (*end != '\0' || val <= 0) {
				fprintf(stderr, "Invalid PID: %s\n", argv[i]);
				return EXIT_FAILURE;
			}
			
			if (pid != 0) {
				fprintf(stderr, "Error: Multiple PIDs specified\n");
				return EXIT_FAILURE;
			}
			
			pid = (pid_t)val;
		}
	}

	/* Verify PID was specified */
	if (pid == 0) {
		fprintf(stderr, "Error: No PID specified\n");
		show_help();
		return EXIT_FAILURE;
	}

	/* Send signal */
	if (kill(pid, sig) < 0) {
		switch (errno) {
			case EPERM:
				fprintf(stderr, "Permission denied to send signal to process %d\n", pid);
				break;
			case ESRCH:
				fprintf(stderr, "No such process: %d\n", pid);
				break;
			case EINVAL:
				fprintf(stderr, "Invalid signal: %d\n", sig);
				break;
			default:
				perror("kill");
		}
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

REGISTER_MODULE(kill);
