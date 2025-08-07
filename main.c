/*
 * main.c - 主程序
 */
#include "module.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define _IS(x) (strcmp(argv[1], x) == 0)

static void show_help() {
	printf("Toolen "VERSION", Copyright (C) ASO-Studio & Xigua\n");
	printf("Usage: toolen [command|options] [Args...]\n");
	printf("Support options: \n"
		"  --help, -h      Show this page\n"
		"  --list, -l      List all support commands\n"
		"  --version, -v   Show version\n\n");
	printf("Support commands: \n[ ");
	list_all_modules();
	printf("]\n");
}

static const char *basename(const char *path) {
	const char *p = strrchr(path, '/');
	if(p) return p+1;
	return path;
}

static char *getRealPath(const char *path) {
	static char p[256];
	ssize_t l =  readlink(path, p, sizeof(p));
	if( l <= 0 ) return NULL;
	return p;
}

static bool iself(const char *b) {
	char *a = getRealPath("/proc/self/exe");
	if(!a) return false;
	if (strcmp(a, b) == 0) {
		return true;
	}
	
	char *s = getRealPath(b);

	if(!s) return false;
	if(strcmp(a, s) == 0) {
		return true;
	}
	return false;
}

int main(int argc, char *argv[]) {
	const char *progname = basename(argv[0]);
	if (find_module(progname) != 0) {
		if(argc <= 1) {
			show_help();
			return 1;
		} else if (find_module(argv[1]) == 0 && argv[1][0] != '-') {
			return run_module(argv[1], argc - 1, &argv[1]);
		} else if (argv[1][0] == '-') {
			if(argv[1][1] == '-') { // "--", --help, --list, e.g
				if(_IS("--help")) {
					show_help();
					return 0;
				} else if (_IS("--list")) {
					list_all_modules();
					return 0;
				} else if (_IS("--version")) {
					printf("Toolen "VERSION"\n");
					return 0;
				} else {
					fprintf(stderr, "Unknown option: %s\n", argv[1]);
					return 1;
				}
			} else {
				switch(argv[1][1]) {
					case 'h':
						show_help();
						return 0;
					case 'l':
						list_all_modules();
						return 0;
					case 'v':
						printf("Toolen "VERSION"\n");
						return 0;
					default:
						fprintf(stderr,"Unknown options: -- '%c'\n", argv[1][1]);
						return 1;
				}
			}
		} else {
			fprintf(stderr, "%s: %s: Command not support\n", progname, argv[1]);
			return 1;
		}
	} else {
		return run_module(progname, argc, argv);
	}
	return 0;
}
