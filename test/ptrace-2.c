/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/types.h>  /* pid_t, waitpid(2), */
#include <sys/ptrace.h> /* ptrace(3), PTRACE_*, */
#include <sys/user.h>   /* struct user*, */
#include <sys/wait.h>   /* waitpid(2), */
#include <stddef.h>     /* offsetof(), */
#include <unistd.h>     /* fork(2), getpid(2), */
#include <errno.h>      /* errno(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <stdlib.h>     /* exit, EXIT_*, */
#include <stdio.h>      /* fprintf(3), stderr, */
#include <stdint.h>     /* *int*_t, */
#include <sys/uio.h>    /* struct iovec */
#include <elf.h>        /* NT_PRSTATUS */

#if !defined(ARCH_X86_64) && !defined(ARCH_ARM_EABI) && !defined(ARCH_X86) && !defined(ARCH_SH4)
#    if defined(__x86_64__)
#        define ARCH_X86_64 1
#    elif defined(__ARM_EABI__)
#        define ARCH_ARM_EABI 1
#    elif defined(__aarch64__)
#        define ARCH_ARM64 1
#    elif defined(__arm__)
#        error "Only EABI is currently supported for ARM"
#    elif defined(__i386__)
#        define ARCH_X86 1
#    elif defined(__SH4__)
#        define ARCH_SH4 1
#    else
#        error "Unsupported architecture"
#    endif
#endif

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

#define REG(regs, index)			\
	(*(unsigned long*) (((uint8_t *) &regs) + reg_offset[index]))

typedef enum {
	SYSARG_NUM = 0,
	SYSARG_1,
	SYSARG_2,
	SYSARG_3,
	SYSARG_4,
	SYSARG_5,
	SYSARG_6,
	SYSARG_RESULT,
	STACK_POINTER,
	INSTR_POINTER,
} Reg;

/* Specify the ABI registers (syscall argument passing, stack pointer).
 * See sysdeps/unix/sysv/linux/${ARCH}/syscall.S from the GNU C Library. */
#if defined(ARCH_X86_64)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rdi),
	[SYSARG_2]      = USER_REGS_OFFSET(rsi),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(r10),
	[SYSARG_5]      = USER_REGS_OFFSET(r8),
	[SYSARG_6]      = USER_REGS_OFFSET(r9),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp),
	[INSTR_POINTER] = USER_REGS_OFFSET(rip),
    };

    static off_t reg_offset_x86[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rbx),
	[SYSARG_2]      = USER_REGS_OFFSET(rcx),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(rsi),
	[SYSARG_5]      = USER_REGS_OFFSET(rdi),
	[SYSARG_6]      = USER_REGS_OFFSET(rbp),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp),
	[INSTR_POINTER] = USER_REGS_OFFSET(rip),
    };

    #undef  REG
    #define REG(regs, index)					\
	(*(unsigned long*) (regs.cs == 0x23			\
		? (((uint8_t *) &regs) + reg_offset_x86[index]) \
		: (((uint8_t *) &regs) + reg_offset[index])))

#elif defined(ARCH_ARM_EABI)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(uregs[7]),
	[SYSARG_1]      = USER_REGS_OFFSET(uregs[0]),
	[SYSARG_2]      = USER_REGS_OFFSET(uregs[1]),
	[SYSARG_3]      = USER_REGS_OFFSET(uregs[2]),
	[SYSARG_4]      = USER_REGS_OFFSET(uregs[3]),
	[SYSARG_5]      = USER_REGS_OFFSET(uregs[4]),
	[SYSARG_6]      = USER_REGS_OFFSET(uregs[5]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(uregs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(uregs[13]),
	[INSTR_POINTER] = USER_REGS_OFFSET(uregs[15]),
    };

#elif defined(ARCH_ARM64)

    #undef  USER_REGS_OFFSET
    #define USER_REGS_OFFSET(reg_name) offsetof(struct user_regs_struct, reg_name)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(regs[8]),
	[SYSARG_1]      = USER_REGS_OFFSET(regs[0]),
	[SYSARG_2]      = USER_REGS_OFFSET(regs[1]),
	[SYSARG_3]      = USER_REGS_OFFSET(regs[2]),
	[SYSARG_4]      = USER_REGS_OFFSET(regs[3]),
	[SYSARG_5]      = USER_REGS_OFFSET(regs[4]),
	[SYSARG_6]      = USER_REGS_OFFSET(regs[5]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(regs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(sp),
	[INSTR_POINTER] = USER_REGS_OFFSET(pc),
    };

#elif defined(ARCH_X86)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_eax),
	[SYSARG_1]      = USER_REGS_OFFSET(ebx),
	[SYSARG_2]      = USER_REGS_OFFSET(ecx),
	[SYSARG_3]      = USER_REGS_OFFSET(edx),
	[SYSARG_4]      = USER_REGS_OFFSET(esi),
	[SYSARG_5]      = USER_REGS_OFFSET(edi),
	[SYSARG_6]      = USER_REGS_OFFSET(ebp),
	[SYSARG_RESULT] = USER_REGS_OFFSET(eax),
	[STACK_POINTER] = USER_REGS_OFFSET(esp),
	[INSTR_POINTER] = USER_REGS_OFFSET(eip),
    };

#elif defined(ARCH_SH4)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(regs[3]),
	[SYSARG_1]      = USER_REGS_OFFSET(regs[4]),
	[SYSARG_2]      = USER_REGS_OFFSET(regs[5]),
	[SYSARG_3]      = USER_REGS_OFFSET(regs[6]),
	[SYSARG_4]      = USER_REGS_OFFSET(regs[7]),
	[SYSARG_5]      = USER_REGS_OFFSET(regs[0]),
	[SYSARG_6]      = USER_REGS_OFFSET(regs[1]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(regs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(regs[15]),
	[INSTR_POINTER] = USER_REGS_OFFSET(pc),
    };

#else

    #error "Unsupported architecture"

#endif

#if defined(PTRACE_GETREGS)
static long read_regs(pid_t pid, struct user_regs_struct *regs)
{
	return ptrace(PTRACE_GETREGS, pid, NULL, regs);
}
#elif defined(PTRACE_GETREGSET)
static long read_regs(pid_t pid, struct user_regs_struct *regs)
{
	struct iovec data;

	data.iov_base = regs;
	data.iov_len = sizeof(struct user_regs_struct);
	return ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &data);
}
#else
#error "PTRACE_GETREGS and PTRACE_GETREGSET not defined"
#endif

int main(int argc, char *argv[])
{
	enum __ptrace_request restart_how;
	int last_exit_status = -1;
	pid_t *pids = NULL;
	long status;
	int signal;
	pid_t pid;

	if (argc <= 1) {
		fprintf(stderr, "Usage: %s /path/to/exe [args]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	pid = fork();
	switch(pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			perror("ptrace(TRACEME)");
			exit(EXIT_FAILURE);
		}

		/* Synchronize with the tracer's event loop.  */
		kill(getpid(), SIGSTOP);

		execvp(argv[1], &argv[1]);
		exit(EXIT_FAILURE);

	default: /* parent */
		break;
	}

	restart_how = (getenv("PTRACER_BEHAVIOR_1") == NULL
		? PTRACE_SYSCALL
		: PTRACE_CONT);

	pids = calloc(1, sizeof(pid_t));
	if (pids == NULL) {
		perror("calloc()");
		exit(EXIT_FAILURE);
	}

	signal = 0;
	while (1) {
		int tracee_status;
		pid_t pid;
		pid_t sid;
		int i;

		/* Wait for the next tracee's stop. */
		pid = waitpid(-1, &tracee_status, __WALL);
		if (pid < 0) {
			perror("waitpid()");
			if (errno != ECHILD)
				exit(EXIT_FAILURE);
			break;
		}

		sid = 0;
		for (i = 0; pids[i] != 0; i++) {
			if (pid == pids[i]) {
				sid = i + 1;
				break;
			}
		}
		if (sid == 0) {
			pids = realloc(pids, (i + 1 + 1) * sizeof(pid_t));
			if (pids == NULL) {
				perror("realloc()");
				exit(EXIT_FAILURE);
			}
			pids[i + 1] = 0;
			pids[i] = pid;
			sid = i + 1;
			fprintf(stderr, "sid %d -> pid %d\n", sid, pid);
		}

		if (WIFEXITED(tracee_status)) {
			fprintf(stderr, "sid %d: exited with status %d\n",
				sid, WEXITSTATUS(tracee_status));
			last_exit_status = WEXITSTATUS(tracee_status);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFSIGNALED(tracee_status)) {
			fprintf(stderr, "sid %d: terminated with signal %d\n",
				sid, WTERMSIG(tracee_status));
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFCONTINUED(tracee_status)) {
			fprintf(stderr, "sid %d: continued\n", sid);
			signal = SIGCONT;
		}
		else if (WIFSTOPPED(tracee_status)) {
			struct user_regs_struct regs;

			/* Don't use WSTOPSIG() to extract the signal
			 * since it clears the PTRACE_EVENT_* bits. */
			signal = (tracee_status & 0xfff00) >> 8;

			switch (signal) {
				static bool skip_bare_sigtrap = false;
				long ptrace_options;

			case SIGTRAP:
				fprintf(stderr, "sid %d: SIGTRAP\n", sid);

				status = read_regs(pid, &regs);
				if (status < 0) {
					fprintf(stderr,
						"sigtrap: ?, ?\n");
				} else {
					fprintf(stderr, "sigtrap: %ld == 0 ? %d\n",
						REG(regs, SYSARG_NUM),
						REG(regs, SYSARG_RESULT) == 0);
				}

				/* PTRACER_BEHAVIOR_1 */
				if (restart_how != PTRACE_SYSCALL) {
					restart_how = PTRACE_SYSCALL;
					signal = 0;
					break;
				}

				/* Distinguish some events from others and
				 * automatically trace each new process with
				 * the same options.
				 *
				 * Note that only the first bare SIGTRAP is
				 * related to the tracing loop, others SIGTRAP
				 * carry tracing information because of
				 * TRACE*FORK/CLONE/EXEC.
				 */
				if (skip_bare_sigtrap) {
					signal = 0;
					break;
				}
				skip_bare_sigtrap = true;

				ptrace_options = PTRACE_O_TRACESYSGOOD
					| PTRACE_O_TRACEFORK
					| PTRACE_O_TRACEVFORK
					| PTRACE_O_TRACEVFORKDONE
					| PTRACE_O_TRACECLONE
					| PTRACE_O_TRACEEXIT;

				if (getenv("PTRACER_BEHAVIOR_2") == NULL)
					ptrace_options |= PTRACE_O_TRACEEXEC;

				status = ptrace(PTRACE_SETOPTIONS, pid, NULL, ptrace_options);
				if (status < 0) {
					perror("ptrace(PTRACE_SETOPTIONS)");
					exit(EXIT_FAILURE);
				}
				/* Fall through. */
			case SIGTRAP | 0x80:
				fprintf(stderr, "sid %d: PTRACE_EVENT_SYSGOOD\n", sid);
				signal = 0;

				status = read_regs(pid, &regs);
				if (status < 0) {
					fprintf(stderr,
						"syscall(?) = ?\n");
				} else {
					fprintf(stderr, "syscall(%ld) == 0 ? %d\n",
						REG(regs, SYSARG_NUM),
						REG(regs, SYSARG_RESULT) == 0);
				}
				break;

			case SIGTRAP | PTRACE_EVENT_VFORK << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_VFORK\n", sid);
				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_VFORK_DONE << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_VFORK\n", sid);
				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_FORK  << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_FORK\n", sid);
				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_CLONE << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_CLONE\n", sid);
				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_EXEC\n", sid);
				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_EXIT  << 8:
				fprintf(stderr, "sid %d: PTRACE_EVENT_EXIT\n", sid);
				signal = 0;
				break;

			case SIGSTOP:
				fprintf(stderr, "sid %d: SIGSTOP\n", sid);
				signal = 0;
				break;

			default:
				break;
			}
		}
		else {
			fprintf(stderr, "sid %d: unknown trace event\n", sid);
			signal = 0;
		}

		/* Restart the tracee and stop it at the next entry or
		 * exit of a system call. */
		status = ptrace(restart_how, pid, NULL, signal);
		if (status < 0)
			fprintf(stderr, "ptrace(<restart_how>, %d, %d): %s\n", sid, signal, strerror(errno));
	}

	return last_exit_status;
}
