/**
 *	kill.c - Kill porcess (SIGTERM by default)
 *
 * 	Created by RoofAlan
 *		2025/8/24
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "module.h"
#include "config.h"

#define _IS(s) (strcasecmp(&argv[1][1], s) == 0)

static void kill_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr,
		"Usage: kill [SIG] pid\n"
		"   --list, -l  list all support signal\n");
}

static void kill_list() {
	printf("HUP INT QUIT ILL TRAP ABRT BUS FPE KILL USR1 SEGV USR2"
		"PIPE ALRM TERM STKFLT CHLD CONT STOP TSTP"
		"TTIN TTOU URG XCPU XFSZ VTALRM PROF WINCH"
		"IO PWR SYS\n");
}

int kill_main(int argc, char *argv[]) {
	if(argc < 2) {
		kill_show_help();
		return 1;
	}

	pid_t pid;

	if(strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
		kill_list();
		return 0;
	} else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		kill_show_help();
		return 0;
	}
	
	if(argv[1][0] == '-') {
		if(!argv[2]) {
			fprintf(stderr, "No pid gave\n");
			return 1;
		}
		pid = atoi(argv[2]);
		if(pid <= 0) {
			fprintf(stderr, "Invalid PID: %d\n", pid);
			return 1;
		}

		int sig = SIGTERM;
		if(_IS("HUP") || _IS("1") || _IS("SIGHUP")) { sig = SIGHUP; }
		if(_IS("INT") || _IS("2") || _IS("SIGINT")) { sig = SIGINT; }
		if(_IS("QUIT") || _IS("3") || _IS("SIGQUIT")) { sig = SIGQUIT; }
		if(_IS("ILL") || _IS("4") || _IS("SIGILL")) { sig = SIGILL; }
		if(_IS("TRAP") || _IS("5") || _IS("SIGTRAP")) { sig = SIGTRAP; }
		if(_IS("ABRT") || _IS("6") || _IS("SIGABRT")) { sig = SIGABRT; }
		if(_IS("BUS") || _IS("7") || _IS("SIGBUS")) { sig = SIGBUS; }
		if(_IS("FPE") || _IS("8") || _IS("SIGFPE")) { sig = SIGFPE; }
		if(_IS("KILL") || _IS("9") || _IS("SIGKILL")) { sig = SIGKILL; }
		if(_IS("USR1") || _IS("10") || _IS("SIGUSR1")) { sig = SIGUSR1; }
		if(_IS("SEGV") || _IS("11") || _IS("SIGSEGV")) { sig = SIGSEGV; }
		if(_IS("USR2") || _IS("12") || _IS("SIGUSR2")) { sig = SIGUSR2; }
		if(_IS("PIPE") || _IS("13") || _IS("SIGPIPE")) { sig = SIGPIPE; }
		if(_IS("ALRM") || _IS("14") || _IS("SIGALRM")) { sig = SIGALRM; }
		if(_IS("TERM") || _IS("15") || _IS("SIGTERM")) { sig = SIGTERM; }
		if(_IS("STKFLT") || _IS("16") || _IS("SIGSTKFLT")) { sig = SIGSTKFLT; }
		if(_IS("CHLD") || _IS("17") || _IS("SIGCHLD")) { sig = SIGCHLD; }
		if(_IS("CONT") || _IS("18") || _IS("SIGCONT")) { sig = SIGCONT; }
		if(_IS("STOP") || _IS("19") || _IS("SIGSTOP")) { sig = SIGSTOP; }
		if(_IS("TSTP") || _IS("20") || _IS("SIGTSTP")) { sig = SIGTSTP; }
		if(_IS("TTIN") || _IS("21") || _IS("SIGTTIN")) { sig = SIGTTIN; }
		if(_IS("TTOU") || _IS("22") || _IS("SIGTTOU")) { sig = SIGTTOU; }
		if(_IS("URG") || _IS("23") || _IS("SIGURG")) { sig = SIGURG; }
		if(_IS("XCPU") || _IS("24") || _IS("SIGXCPU")) { sig = SIGXCPU; }
		if(_IS("XFSZ") || _IS("25") || _IS("SIGXFSZ")) { sig = SIGXFSZ; }
		if(_IS("VTALRM") || _IS("26") || _IS("SIGVTALRM")) { sig = SIGVTALRM; }
		if(_IS("PROF") || _IS("27") || _IS("SIGPROF")) { sig = SIGPROF; }
		if(_IS("WINCH") || _IS("28") || _IS("SIGWINCH")) { sig = SIGWINCH; }
		if(_IS("IO") || _IS("29") || _IS("SIGIO")) { sig = SIGIO; }
		if(_IS("PWR") || _IS("30") || _IS("SIGPWR")) { sig = SIGPWR; }
		if(_IS("SYS") || _IS("31") || _IS("SIGSYS")) { sig = SIGSYS; }

		if (sig == -1) {
			fprintf(stderr, "Unknown signal -- '%s'\n", argv[1]);
			fprintf(stderr, "try pass '-l' for signal list\n");
			return 1;
		}
	
		if(kill(pid, sig) < 0) {
			perror("kill");
			return 1;
		}
		return 0;
	}
	return 0;
}

REGISTER_MODULE(kill);
