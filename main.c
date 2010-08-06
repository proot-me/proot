/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 * Inspired by: strace.
 */

#include <unistd.h>     /* fork(2), chdir(2), exec*(3), */
#include <stdlib.h>     /* exit(3), EXIT_*, */
#include <stdio.h>      /* puts(3), */
#include <sys/wait.h>   /* wait(2), */
#include <sys/ptrace.h> /* ptrace(2), */
#include <limits.h>     /* PATH_MAX, */
#include <string.h>     /* strcmp(3), */

#include "path.h"     /* init_path_translator(), */
#include "child.h"    /* init_children_info(), */
#include "syscall.h"  /* translate_syscall(), */

static void print_usage(void)
{
	puts("Usage:\n");
	puts("\tproot [options] <new_root> <program> [args]\n");
	puts("Where options are:\n");
	puts("\tnot yet supported\n");
}

int main(int argc, char *argv[])
{
	int child_status;
	long status;
	int signal;
	pid_t pid;

	if (argc < 3) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	init_path_translator(argv[1]);
	init_children_info(64);

	pid = fork();
	switch(pid) {
	case -1:
		perror("proot -- fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */

		/* Declare myself as ptraceable before executing the
		 * requested program. */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			perror("proot -- ptrace(TRACEME)");
			exit(EXIT_FAILURE);
		}

		/* Ensure the child starts in a valid cwd within the
		 * new root. */
		status = chdir(argv[1]);
		if (status < 0) {
			perror("proot -- chdir()");
			exit(EXIT_FAILURE);
		}
		status = setenv("PWD", "/", 1);
		if (status < 0) {
			perror("proot -- setenv()");
			exit(EXIT_FAILURE);
		}
		status = setenv("OLDPWD", "/", 1);
		if (status < 0) {
			perror("proot -- setenv()");
			exit(EXIT_FAILURE);
		}

		/* Here we go! */
		status = execvp(argv[2], argv + 2);
		perror("proot -- execvp()");
		exit(EXIT_FAILURE);

	default: /* parent */
		if (new_child(pid) == NULL)
			exit(EXIT_FAILURE);
		break;
	}

	signal = 0;
	while (get_nb_children() > 0) {
		/* Wait for the next child's stop. */
		pid = wait(&child_status);
		if (pid < 0) {
			perror("proot -- wait()");
			exit(EXIT_FAILURE);
		}

		/* Check every child file descriptors. */
		foreach_child(check_fd);

		if (WIFEXITED(child_status)) {
			fprintf(stderr, "proot: child %d exited with result %d\n",
				pid, WEXITSTATUS(child_status));
			delete_child(pid);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFSIGNALED(child_status)) {
			fprintf(stderr, "proot: child %d terminated with signal %d\n",
				pid, WTERMSIG(child_status));
			delete_child(pid);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFCONTINUED(child_status)) {
			fprintf(stderr, "proot: child %d continued\n", pid);
			signal = SIGCONT;
		}
		else if (WIFSTOPPED(child_status)) {

			/* Don't WSTOPSIG() to extract the signal
			 * since it clears the PTRACE_EVENT_* bits. */
			signal = (child_status & 0xfff00) >> 8;

			switch (signal) {
			case SIGTRAP | 0x80:
				translate_syscall(pid);
				signal = 0;
				break;

			case SIGTRAP:
				/* Distinguish some events from others and
				 * automatically trace each new process with
				 * the same options.  Note: only the first
				 * process should come here. */
				status = ptrace(PTRACE_SETOPTIONS, pid, NULL,
						PTRACE_O_TRACESYSGOOD |
						PTRACE_O_TRACEFORK    |
						PTRACE_O_TRACEVFORK   |
						PTRACE_O_TRACECLONE   |
						PTRACE_O_TRACEEXEC);
				if (status < 0) {
					perror("proot -- ptrace(PTRACE_SETOPTIONS)");
					exit(EXIT_FAILURE);
				}

				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			case SIGTRAP | PTRACE_EVENT_CLONE << 8:
			case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
				/* Ignore these signals. */
				signal = 0;
				break;

			default:
				/* Propagate all other signals. */
				break;
			}
		}
		else {
			fprintf(stderr, "proot: unknown trace event\n");
			signal = 0;
		}

		/* Restart the child and stop it at the next entry or exit of
		 * a system call. */
		status = ptrace(PTRACE_SYSCALL, pid, NULL, signal);
		if (status < 0) {
			perror("proot -- ptrace(SYSCALL)");
			exit(EXIT_FAILURE);
		}
	}

	fprintf(stderr, "proot: exited.\n");
	exit(EXIT_SUCCESS);
}
