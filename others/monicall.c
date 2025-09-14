#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
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

typedef unsigned long long int regval_t;
typedef void (*CatcherFunc_t)(int, regval_t *args);

typedef struct MoniStruct {
	CatcherFunc_t Catcher;
	int Sysnum;
	struct MoniStruct *next;
} MoniCall;

// Definitions for x86_64
#if defined(__x86_64__)

# define SYSCALL_RETVAL  rax
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

# define SYSCALL_RETVAL  regs[0]
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

	char *data = xmalloc(size + 1);
	if (!data) return NULL;

	// Read by bytes
	for (size_t i = 0; i < size; i++) {
		errno = 0;
		long val = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);
		if (val == -1 && errno != 0) {
			xfree(data);
			return NULL;
		}
		data[i] = (char)(val & 0xFF); // Get low byte
	}
	data[size] = '\0';
	return data;
}

// Read string from memory
static char *read_string_data(pid_t pid, void *addr) {
	if (addr == NULL) return NULL;

	long val = 1;
	size_t count = 0;
	size_t si = 1024;
	char *data = xmalloc(1024);
	if (!data) return NULL;

	// Read by bytes
	for (count = 0; (val = ptrace(PTRACE_PEEKDATA, pid, addr + count, NULL)) != 0; count++) {
		if (count >= si) {
			si += 1024;
			data = xrealloc(data, si);
		}

		errno = 0;
		if (val == -1 && errno != 0) {
			xfree(data);
			return NULL;
		}
		data[count] = (char)(val & 0xFF); // Get low byte
	}

	data[count] = '\0';
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

			regval_t args[6];
			args[0] = regs.ARG1_REG;
			args[1] = regs.ARG2_REG;
			args[2] = regs.ARG3_REG;
			args[3] = regs.ARG4_REG;
			args[4] = regs.ARG5_REG;
			args[5] = regs.ARG6_REG;
			if (catcher) {
				catcher(child_pid, args);
			}

			// Continue
			ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);
			waitpid(child_pid, &status, 0);

			// Wait for syscall exit
			if (!WIFSTOPPED(status) || WSTOPSIG(status) != (SIGTRAP | 0x80)) {
				printf("  = ?\n");
				break;
			}

			// Get registers
			if (ptrace(PTRACE_GETREGSET, child_pid, NT_PRSTATUS, &iov) == -1) break;

			if (catcher) {
				long retd = regs.SYSCALL_RETVAL;
				if (retd > 999) {
					printf("\t  = 0x%lx\n", retd);
				} else {
					printf("\t  = %ld\n", retd);
				}
			}
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

// Catch: write
static void catch_write(pid_t pid, regval_t *args) {
	int fd = args[0];
	void *buf = (void *)args[1];
	size_t size = args[2];

	char *data = read_data(pid,buf,size);

	char *escaped_data = xmalloc(size * 4 + 1);
	if (escaped_data == NULL) {
		printf("write(%d, \"%.*s\", %zu)", fd, (int)size, data, size);
		xfree(data);
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
		printf("==> write(%d, \"%.64s...\", %zu)", fd, escaped_data, size);
	} else {
		printf("==> write(%d, \"%s\", %zu)", fd, escaped_data, size);
	}

	xfree(data);
	xfree(escaped_data);
}

// Catch: read
static void catch_read(int pid, regval_t *args) {
	(void)pid;
	int fd = args[0];
	void *buf = (void*)args[1];
	size_t size = args[3];
	printf("==> read(%d, %p, %zu)", fd, buf, size);
}

#ifdef SYS_open
// Catch: open
static void catch_open(pid_t pid, regval_t *args) {
	unsigned long pathname_addr = args[0];
	int flags = args[1];
	int mode = args[2];
	char pathname[256] = {0};
	int valid = 1;

	if (pathname_addr < 0x1000 || pathname_addr >= 0x800000000000) {
		valid = 0;
	} else {
		for (unsigned long i = 0; i < sizeof(pathname) - 1; ) {
			unsigned long addr = pathname_addr + i;
			addr &= ~(sizeof(long) - 1);
			errno = 0;
			long val = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, NULL);
			if (val == -1 && errno != 0) {
				valid = 0;
				break;
			}

			for (unsigned long j = 0; j < sizeof(long) && i < sizeof(pathname)-1; j++, i++) {
				char c = (char)((val >> (j * 8)) & 0xFF);
				if (c == '\0') goto done;
				pathname[i] = c;
			}
		}
	}

done:
	if (valid && pathname[0] != '\0') {
		printf("==> open(\"%s\", 0x%x, 0%o)", pathname, flags, mode);
	} else {
		printf("==> open(0x%lx, 0x%x, 0%o)", pathname_addr, flags, mode);
	}
}
#endif // SYS_open

// Catch: exit
static void catch_exit(pid_t pid, regval_t *args) {
	(void)pid;
	int status = (int)args[0];
	printf("==> exit(%d)", status);
}

// Catch: exit_group
static void catch_exit_group(pid_t pid, regval_t *args) {
	(void)pid;
	int status = (int)args[0];
	printf("==> exit_group(%d)", status);
}

// Catch: openat
static void catch_openat(pid_t pid, regval_t *args) {
	int cwd = args[0];
	unsigned long pathname_addr = args[1];
	int flags = args[2];
	int mode = args[3];

	char pathname[256] = {0};
	int valid = 1;

	if (pathname_addr < 0x1000 || pathname_addr >= 0x800000000000) {
		valid = 0;
	} else {
		for (unsigned long i = 0; i < sizeof(pathname) - 1; ) {
			unsigned long addr = pathname_addr + i;
			addr &= ~(sizeof(long) - 1);
			errno = 0;
			long val = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, NULL);
			if (val == -1 && errno != 0) {
				valid = 0;
				break;
			}

			for (unsigned long j = 0; j < sizeof(long) && i < sizeof(pathname)-1; j++, i++) {
				char c = (char)((val >> (j * 8)) & 0xFF);
				if (c == '\0') goto done;
				pathname[i] = c;
			}
		}
	}

done:
	if (valid && pathname[0] != '\0') {
		printf("==> openat(%d, \"%s\", 0x%x", cwd, pathname, flags);
	} else {
		printf("==> openat(%d, 0x%lx, 0x%x", cwd, pathname_addr, flags);
	}

	if (flags & O_CREAT) {
		printf(", %04d", mode);
	}
	printf(")");
}

// Catch: close
static void catch_close(int pid, regval_t *args) {
	(void)pid;
	int fd = args[0];
	printf("==> close(%d)", fd);
}

// Catch: brk
static void catch_brk(int pid, regval_t *args) {
	(void)pid;
	if (args[0] != 0) {
		printf("==> brk(%llx)", args[0]);
	} else {
		printf("==> brk(NULL)");
	}
}

#ifdef SYS_access
// Catch: access
static void catch_access(int pid, regval_t *args) {
	void *buf = (void*)args[0];
	int flag = args[1];

	char *path = read_string_data(pid, buf);
	if (path) {
		printf("==> access(\"%s\", %d)", path, flag);
		xfree(path);
	} else {
		printf("==> access(%p, %d)", path, flag);
	}
}
#endif // SYS_access

// Catch: faccessat
static void catch_faccessat(int pid, regval_t *args) {
	int cwd = args[0];
	void *buf = (void*)args[1];
	int flag = args[2];

	char *path = read_string_data(pid, buf);
	if (path) {
		printf("==> faccessat(%d, \"%s\", %d)", cwd, path, flag);
		xfree(path);
	} else {
		printf("==> faccessat(%d, %p, %d)", cwd, path, flag);
	}
}

// Catch: lseek
static void catch_lseek(int pid, regval_t *args) {
	(void)pid;
	int fd = args[0];
	off_t off = args[1];
	int whence = args[2];

	printf("==> lseek(%d, %ld, %d)", fd, off, whence);
}

// Catch: mmap
static void catch_mmap(int pid, regval_t *args) {
	(void)pid;
	regval_t addr = args[0];
	size_t len = args[1];
	int prot = args[2];
	int flags = args[3];
	int fd = args[4];
	off_t off = args[5];

	if (addr != 0) {
		printf("==> mmap(0x%llx", addr);
	} else {
		printf("==> mmap(NULL");
	}
	printf(", %zu, %d, %d, %d, %ld)", len, prot, flags, fd, off);
}

// Catch: munmap
static void catch_munmap(int pid, regval_t *args) {
	(void)pid;
	regval_t addr = args[0];
	size_t size = args[1];

	printf("==> munmap(0x%llx, %zu)", addr, size);
}

// Catch: mlock
static void catch_mlock(int pid, regval_t *args) {
	(void)pid;
	regval_t addr = args[0];
	size_t size = args[1];

	printf("==> mlock(0x%llx, %zu)", addr, size);
}

// Catch: munlock
static void catch_munlock(int pid, regval_t *args) {
	(void)pid;
	regval_t addr = args[0];
	size_t size = args[1];

	printf("==> munlock(0x%llx, %zu)", addr, size);
}

// Catch: mlockall
static void catch_mlockall(int pid, regval_t *args) {
	(void)pid;
	int flags = args[0];

	printf("==> mlockall(%d)", flags);
}

// Catch: munlockall
static void catch_munlockall(int pid, regval_t *args) {
	(void)pid; (void)args;
	printf("==> munlockall()");
}

// Catch: mprotect
static void catch_mprotect(int pid, regval_t *args) {
	(void)pid;
	regval_t addr = args[0];
	size_t size = args[1];
	int prot = args[2];
	printf("==> mprotect(0x%llx, %zu, %d)", addr, size, prot);
}

M_ENTRY(monicall) {
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
	addCatcher(&m, SYS_brk, catch_brk);
	addCatcher(&m, SYS_lseek, catch_lseek);
	addCatcher(&m, SYS_mmap, catch_mmap);
	addCatcher(&m, SYS_munmap, catch_munmap);
	addCatcher(&m, SYS_mlock, catch_mlock);
	addCatcher(&m, SYS_munlock, catch_munlock);
	addCatcher(&m, SYS_mlockall, catch_mlockall);
	addCatcher(&m, SYS_munlockall, catch_munlockall);
	addCatcher(&m, SYS_mprotect, catch_mprotect);
	addCatcher(&m, SYS_faccessat, catch_faccessat);
#ifdef SYS_open	// On some devices(such as Android(Aarch64), .e.g, they dont have SYS_open)
	addCatcher(&m, SYS_open, catch_open);
#endif
#ifdef SYS_access
	addCatcher(&m, SYS_access, catch_access);
#endif

	startMonit(argv[1], &argv[1], m);

	cleanupMoniCall(m);
	return 0;
}

#else // !defined(__NO_SYS_USER_H)
	#warning does not support on this platform
	M_ENTRY(monicall) {
		pplog(P_NAME, "does not support on this platform");
		return 1;
	}
#endif // defined(__NO_SYS_USER_H)

REGISTER_MODULE(monicall);
