#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>

// Check if some platforms do not have <sys/user.h>
#if __has_include(<sys/user.h>)
# include <sys/user.h>
#else
# define __NO_SYS_USER_H 1
#endif

#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <elf.h>

#include "config.h"
#include "module.h"
#include "lib.h"
#include "debug.h"

#ifndef __NO_SYS_USER_H

typedef void (*CatcherFunc_t)(int, struct user_regs_struct*);

typedef struct MoniStruct {
	CatcherFunc_t Catcher;
	int Sysnum;
	struct MoniStruct *next;
} MoniCall;

// Definitions for x86_64
#if defined(__x86_64__)

# define SYSCALL_NUM_REG orig_rax
# define ARG1_REG rdi
# define ARG2_REG rsi
# define ARG3_REG rdx
# define ARG4_REG r10
# define ARG5_REG r8
# define ARG6_REG r9

# define SYSCALL_NUM32 eax
# define ARG1_REG32 ebx
# define ARG2_REG32 ecx
# define ARG3_REG32 edx
# define ARG4_REG32 esi
# define ARG5_REG32 edi
# define ARG6_REG32 ebp

// Definitions for aarch64
#elif defined(__aarch64__)

# define SYSCALL_NUM_REG regs[8]
# define ARG1_REG regs[0]
# define ARG2_REG regs[1]
# define ARG3_REG regs[2]
# define ARG4_REG regs[3]
# define ARG5_REG regs[4]
# define ARG6_REG regs[5]

// Others: unsupported
#else
# error "Unsupported architecture, please disable this command"
#endif

// Add a syscall catcher
static void addCatcher(MoniCall **m, int num, CatcherFunc_t f) {
	LOG("Adding catcher for syscall number: %d\n", num);
	MoniCall *next = *m;

	*m = (MoniCall*)xmalloc(sizeof(MoniCall));
	(*m)->Sysnum = num;
	(*m)->Catcher = f;

	(*m)->next = next;
	return;
}

static CatcherFunc_t findCatcher(MoniCall *m, int num) {
	LOG("Finding catcher: %d\n", num);

	if (!m) return NULL;

	MoniCall *mc = m;
	while (mc) {
		if (mc->Sysnum == num) {
			LOG("Returned catcher: %p\n", mc->Catcher);
			return mc->Catcher;
		}
		LOG("Skipped: %d\n", mc->Sysnum);
		mc = mc->next;
	}
	LOG("No catcher found for syscall '%d'\n", num);
	return NULL;
}

static void cleanupMoniCall(MoniCall *m) {
	MoniCall *cur = m;
	while (cur) {
		xfree(cur);
		cur = cur->next;
	}
}

// Read data from memory
static char *read_data(pid_t pid, void *addr, size_t size) {
	if (addr == NULL || size == 0) return NULL;

	char *data = malloc(size + 1);
	if (!data) return NULL;

	// Read by bytes
	for (size_t i = 0; i < size; i++) {
		errno = 0;
		long val = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);
		if (val == -1 && errno != 0) {
			free(data);
			return NULL;
		}
		data[i] = (char)(val & 0xFF); // Get low byte
	}
	data[size] = '\0';
	return data;
}

// Set register value
static fused int set_register(pid_t pid, struct user_regs_struct *regs) {
	struct iovec iov = {
		.iov_base = regs,
		.iov_len = sizeof(*regs)
	};
	return ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

// Catch syscall: write
static void catch_write(pid_t pid, struct user_regs_struct *regs) {
	int fd = regs->ARG1_REG;
	void *buf = (void *)regs->ARG2_REG;
	size_t size = regs->ARG3_REG;

	char *data = read_data(pid,buf,size);

	char *escaped_data = malloc(size * 4 + 1);
	if (escaped_data == NULL) {
		printf("write(%d, \"%.*s\", %zu)\n", fd, (int)size, data, size);
		free(data);
		return;
	}

	int pos = 0;
	for (size_t i = 0; i < size; i++) {
		if (data[i] == '\n') {
			escaped_data[pos++] = '\\';
			escaped_data[pos++] = 'n';
		} else if (data[i] == '\t') {
			escaped_data[pos++] = '\\';
			escaped_data[pos++] = 't';
		} else if (data[i] < 32 || data[i] >= 127) {
			pos += sprintf(escaped_data + pos, "\\x%02x", (unsigned char)data[i]);
		} else {
			escaped_data[pos++] = data[i];
		}
	}
	escaped_data[pos] = '\0';

	// Print result
	if (size > 64) {
		printf("==> write(%d, \"%.64s...\", %zu)\n", fd, escaped_data, size);
	} else {
		printf("==> write(%d, \"%s\", %zu)\n", fd, escaped_data, size);
	}

	free(data);
	free(escaped_data);
}

void catch_read(int pid, struct user_regs_struct *regs) {
	int fd = regs->ARG1_REG;
	void *buf = (void*)regs->ARG2_REG;
	size_t size = regs->ARG3_REG;
	printf("==> read(%d, %p, %zu)\n", fd, buf, size);
}
fused static void catch_open(pid_t pid, struct user_regs_struct *regs) {
	unsigned long pathname_addr = regs->ARG1_REG;
	int flags = regs->ARG2_REG;
	int mode = regs->ARG3_REG;
	char pathname[256] = {0};
	int valid = 1;

	if (pathname_addr < 0x1000 || pathname_addr >= 0x800000000000) {
		valid = 0;
	} else {
		for (int i = 0; i < sizeof(pathname) - 1; ) {
			unsigned long addr = pathname_addr + i;
			addr &= ~(sizeof(long) - 1);
			errno = 0;
			long val = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, NULL);
			if (val == -1 && errno != 0) {
				valid = 0;
				break;
			}

			for (int j = 0; j < sizeof(long) && i < sizeof(pathname)-1; j++, i++) {
				char c = (char)((val >> (j * 8)) & 0xFF);
				if (c == '\0') goto done;
				pathname[i] = c;
			}
		}
	}

done:
	if (valid && pathname[0] != '\0') {
		printf("==> open(\"%s\", 0x%x, 0%o)\n", pathname, flags, mode);
	} else {
		printf("==> open(0x%lx, 0x%x, 0%o)\n", pathname_addr, flags, mode);
	}
}


static void catch_exit(pid_t pid, struct user_regs_struct *regs) {
	int status = (int)regs->ARG1_REG;
	printf("==> exit(%d)\n", status);
}

static void catch_exit_group(pid_t pid, struct user_regs_struct *regs) {
	int status = (int)regs->ARG1_REG;
	printf("==> exit_group(%d)\n", status);
}

static void catch_openat(pid_t pid, struct user_regs_struct *regs) {
	unsigned long cwd = regs->ARG1_REG;
	unsigned long pathname_addr = regs->ARG2_REG;
	int flags = regs->ARG3_REG;
	//int mode = regs->ARG4_REG;
	char pathname[256] = {0};
	int valid = 1;

	if (pathname_addr < 0x1000 || pathname_addr >= 0x800000000000) {
		valid = 0;
	} else {
		for (int i = 0; i < sizeof(pathname) - 1; ) {
			unsigned long addr = pathname_addr + i;
			addr &= ~(sizeof(long) - 1);
			errno = 0;
			long val = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, NULL);
			if (val == -1 && errno != 0) {
				valid = 0;
				break;
			}

			for (int j = 0; j < sizeof(long) && i < sizeof(pathname)-1; j++, i++) {
				char c = (char)((val >> (j * 8)) & 0xFF);
				if (c == '\0') goto done;
				pathname[i] = c;
			}
		}
	}

done:
	if (valid && pathname[0] != '\0') {
		printf("==> openat(0x%lx, \"%s\", 0x%x)\n", cwd, pathname, flags);
	} else {
		printf("==> openat(0x%lx, 0x%lx, 0x%x)\n", cwd, pathname_addr, flags);
	}
}

static void catch_close(int pid, struct user_regs_struct *regs) {
	int fd = regs->ARG1_REG;
	printf("==> close(%d)\n", fd);
}

static int startMonit(const char *filename, char **argv, MoniCall *m) {
	pid_t child_pid = fork();
	if (child_pid == -1) {
		perror("fork failed");
		return 1;
	}

	if (child_pid == 0) { // Sub process: execute target program
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execvp(filename, argv);
		perror("execvp failed");
		exit(1);
	} else { // Parent process: debugger
		int status;
		struct user_regs_struct regs;
		struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };

		waitpid(child_pid, &status, 0);
		ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACESYSGOOD);

		while (1) {
			// Wait for stopping (Child process)
			if (ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL) == -1) break;
			waitpid(child_pid, &status, 0);
			if (!WIFSTOPPED(status) || WSTOPSIG(status) != (SIGTRAP | 0x80)) break;

			// Get registers
			if (ptrace(PTRACE_GETREGSET, child_pid, NT_PRSTATUS, &iov) == -1) break;

			// Catch syscall
			long syscall_num = regs.SYSCALL_NUM_REG;

			// Execute catcher
			CatcherFunc_t catcher = findCatcher(m, syscall_num);
			if (catcher) {
				catcher(child_pid, &regs);
			}

			// Continue
			ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);
			waitpid(child_pid, &status, 0);
		}

		// Detach
		ptrace(PTRACE_DETACH, child_pid, NULL, NULL);
	}

	return 0;
}

static void monicall_show_help() {
	SHOW_VERSION(stderr);
	fprintf(stderr, "Usage: monicall PROGRAM [ARGS]...\n\n"
			"Captures system calls that are called when the program is executed\n");
}

int monicall_main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: monicall PROGRAM [ARGS]...\n"
				"Try pass '--help' for more details\n");
		return 1;
	}

	if (findArg(argv, argc, "--help")) {
		monicall_show_help();
		return 0;
	}

	MoniCall *m = NULL;

	// TODO: Add more catcher
	addCatcher(&m, SYS_write, catch_write);
	addCatcher(&m, SYS_close, catch_close);
	addCatcher(&m, SYS_openat, catch_openat);
	addCatcher(&m, SYS_read, catch_read);
	addCatcher(&m, SYS_exit, catch_exit);
	addCatcher(&m, SYS_exit_group, catch_exit_group);
#ifdef SYS_open	// On some devices(such as Android(Aarch64), .e.g, they dont have SYS_open)
	addCatcher(&m, SYS_open, catch_open);
#endif

	startMonit(argv[1], &argv[1], m);

	cleanupMoniCall(m);
	return 0;
}

#else // !defined(__NO_SYS_USER_H)
	#warning does not support on this platform
	int monicall_main(int argc, char *argv[]) {
		pplog(P_NAME, "does not support on this platform");
		return 1;
	}
#endif // defined(__NO_SYS_USER_H)

REGISTER_MODULE(monicall);
