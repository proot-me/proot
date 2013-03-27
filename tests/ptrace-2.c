/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2013 STMicroelectronics
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

#include <sys/types.h>  /* pid_t, */
#include <sys/ptrace.h> /* ptrace(3), PTRACE_*, */
#include <sys/types.h>  /* waitpid(2), */
#include <sys/wait.h>   /* waitpid(2), */
#include <unistd.h>     /* fork(2), getpid(2), */
#include <errno.h>      /* errno(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <stdlib.h>     /* exit, EXIT_*, */
#include <stdio.h>      /* fprintf(3), stderr, */

int main(int argc, char *argv[])
{
	int last_exit_status = -1;
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

	signal = 0;
	while (1) {
		int tracee_status;
		pid_t pid;

		/* Wait for the next tracee's stop. */
		pid = waitpid(-1, &tracee_status, __WALL);
		if (pid < 0) {
			if (errno != ECHILD) {
				perror("waitpid()");
				exit(EXIT_FAILURE);
			}
			break;
		}

		if (WIFEXITED(tracee_status)) {
			fprintf(stderr, "pid %d: exited with status %d\n",
				pid, WEXITSTATUS(tracee_status));
			last_exit_status = WEXITSTATUS(tracee_status);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFSIGNALED(tracee_status)) {
			fprintf(stderr, "pid %d: terminated with signal %d\n",
				pid, WTERMSIG(tracee_status));
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFCONTINUED(tracee_status)) {
			fprintf(stderr, "pid %d: continued\n", pid);
			signal = SIGCONT;
		}
		else if (WIFSTOPPED(tracee_status)) {
			/* Don't use WSTOPSIG() to extract the signal
			 * since it clears the PTRACE_EVENT_* bits. */
			signal = (tracee_status & 0xfff00) >> 8;

			switch (signal) {
				static bool skip_bare_sigtrap = false;

			case SIGTRAP:
				/* Distinguish some events from others and
				 * automatically trace each new process with
				 * the same options.
				 *
				 * Note that only the first bare SIGTRAP is
				 * related to the tracing loop, others SIGTRAP
				 * carry tracing information because of
				 * TRACE*FORK/CLONE/EXEC.
				 */
				if (skip_bare_sigtrap)
					break;
				skip_bare_sigtrap = true;

				status = ptrace(PTRACE_SETOPTIONS, pid, NULL,
						PTRACE_O_TRACESYSGOOD |
						PTRACE_O_TRACEFORK    |
						PTRACE_O_TRACEVFORK   |
						PTRACE_O_TRACEEXEC    |
						PTRACE_O_TRACECLONE   |
						PTRACE_O_TRACEEXIT);
				if (status < 0) {
					perror("ptrace(PTRACE_SETOPTIONS)");
					exit(EXIT_FAILURE);
				}
				/* Fall through. */
			case SIGTRAP | 0x80:
			case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			case SIGTRAP | PTRACE_EVENT_CLONE << 8:
			case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
			case SIGTRAP | PTRACE_EVENT_EXIT  << 8:
			case SIGSTOP:
				signal = 0;
				break;

			default:
				break;
			}
		}
		else {
			fprintf(stderr, "unknown trace event\n");
			signal = 0;
		}

		/* Restart the tracee and stop it at the next entry or
		 * exit of a system call. */
		status = ptrace(PTRACE_SYSCALL, pid, NULL, signal);
		if (status < 0)
			fprintf(stderr, "ptrace(SYSCALL, %d, %d)\n", pid, signal);
	}

	return last_exit_status;
}
