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

#include <sched.h>      /* CLONE_*,  */
#include <sys/types.h>  /* pid_t, */
#include <sys/ptrace.h> /* ptrace(1), PTRACE_*, */
#include <sys/types.h>  /* waitpid(2), */
#include <sys/wait.h>   /* waitpid(2), */
#include <sys/personality.h> /* personality(2), ADDR_NO_RANDOMIZE, */
#include <sys/utsname.h> /* uname(2), */
#include <unistd.h>     /* fork(2), chdir(2), getpid(2), */
#include <string.h>     /* strcmp(3), */
#include <errno.h>      /* errno(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <assert.h>     /* assert(3), */
#include <stdlib.h>     /* atexit(3), getenv(3), */
#include <talloc.h>     /* talloc_*, */

#include "tracee/event.h"
#include "cli/notice.h"
#include "path/path.h"
#include "path/binding.h"
#include "syscall/syscall.h"
#include "syscall/seccomp.h"
#include "extension/extension.h"
#include "execve/elf.h"

#include "attribute.h"
#include "compat.h"

/**
 * Launch the first process as specified by @tracee->cmdline[].  This
 * function returns -errno if an error occurred, otherwise 0.
 */
int launch_process(Tracee *tracee)
{
	long status;
	pid_t pid;

	pid = fork();
	switch(pid) {
	case -1:
		notice(tracee, ERROR, SYSTEM, "fork()");
		return -errno;

	case 0: /* child */
		/* Declare myself as ptraceable before executing the
		 * requested program. */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			notice(tracee, ERROR, SYSTEM, "ptrace(TRACEME)");
			return -errno;
		}

		/* Warn about open file descriptors. They won't be
		 * translated until they are closed. */
		if (tracee->verbose > 0)
			list_open_fd(tracee);

		/* RHEL4 uses an ASLR mechanism that creates conflicts
		 * between the layout of QEMU and the layout of the
		 * target program.  Moreover it's easier to debug
		 * PRoot with tracees' ASLR turned off.  */
		status = personality(0xffffffff);
		if (   status < 0
		    || personality(status | ADDR_NO_RANDOMIZE) < 0)
			notice(tracee, WARNING, INTERNAL, "can't disable ASLR");

		/* Synchronize with the tracer's event loop.  Without
		 * this trick the tracer only sees the "return" from
		 * the next execve(2) so PRoot wouldn't handle the
		 * interpreter/runner.  I also verified that strace
		 * does the same thing. */
		kill(getpid(), SIGSTOP);

		/* Improve performance by using seccomp mode 2, unless
		 * this support is explicitly disabled.  */
		if (getenv("PROOT_NO_SECCOMP") == NULL)
			(void) enable_syscall_filtering(tracee);

		/* Now process is ptraced, so the current rootfs is already the
		 * guest rootfs.  Note: Valgrind can't handle execve(2) on
		 * "foreign" binaries (ENOEXEC) but can handle execvp(3) on such
		 * binaries.  */
		execvp(tracee->exe, tracee->cmdline);
		return -errno;

	default: /* parent */
		/* We know the pid of the first tracee now.  */
		tracee->pid = pid;
		return 0;
	}

	/* Never reached.  */
	return -ENOSYS;
}

/* Send the KILL signal to all tracees when PRoot has received a fatal
 * signal.  */
static void kill_all_tracees2(int signum, siginfo_t *siginfo UNUSED, void *ucontext UNUSED)
{
	notice(NULL, WARNING, INTERNAL, "signal %d received from process %d",
		signum, siginfo->si_pid);
	kill_all_tracees();

	/* Exit immediately for system signals (segmentation fault,
	 * illegal instruction, ...), otherwise exit cleanly through
	 * the event loop.  */
	if (signum != SIGQUIT)
		_exit(EXIT_FAILURE);
}

/* Print on stderr the complete talloc hierarchy.  */
static void print_talloc_hierarchy(int signum, siginfo_t *siginfo UNUSED, void *ucontext UNUSED)
{
	void print_talloc_chunk(const void *ptr, int depth, int max_depth UNUSED,
				int is_ref, void *data UNUSED)
	{
		const char *name;
		size_t count;
		size_t size;

		name = talloc_get_name(ptr);
		size = talloc_get_size(ptr);
		count = talloc_reference_count(ptr);

		if (depth == 0)
			return;

		while (depth-- > 1)
			fprintf(stderr, "\t");

		fprintf(stderr, "%-16s ", name);

		if (is_ref)
			fprintf(stderr, "-> %-8p", ptr);
		else {
			fprintf(stderr, "%-8p  %zd bytes  %zd ref'", ptr, size, count);

			if (name[0] == '$') {
				fprintf(stderr, "\t(\"%s\")", (char *)ptr);
			}
			if (name[0] == '@') {
				char **argv;
				int i;

				fprintf(stderr, "\t(");
				for (i = 0, argv = (char **)ptr; argv[i] != NULL; i++)
					fprintf(stderr, "\"%s\", ", argv[i]);
				fprintf(stderr, ")");
			}
			else if (strcmp(name, "Tracee") == 0) {
				fprintf(stderr, "\t(pid = %d)", ((Tracee *)ptr)->pid);
			}
			else if (strcmp(name, "Bindings") == 0) {
				 Tracee *tracee;

				 tracee = TRACEE(ptr);

				 if (ptr == tracee->fs->bindings.pending)
					 fprintf(stderr, "\t(pending)");
				 else if (ptr == tracee->fs->bindings.guest)
					 fprintf(stderr, "\t(guest)");
				 else if (ptr == tracee->fs->bindings.host)
					 fprintf(stderr, "\t(host)");
			}
			else if (strcmp(name, "Binding") == 0) {
				Binding *binding = (Binding *)ptr;
				fprintf(stderr, "\t(%s:%s)", binding->host.path, binding->guest.path);
			}
		}

		fprintf(stderr, "\n");
	}

	switch (signum) {
	case SIGUSR1:
		talloc_report_depth_cb(NULL, 0, 100, print_talloc_chunk, NULL);
		break;

	case SIGUSR2:
		talloc_report_depth_file(NULL, 0, 100, stderr);
		break;

	default:
		break;
	}
}

/**
 * Check if this instance of PRoot can *technically* handle @tracee.
 */
static void check_architecture(Tracee *tracee)
{
	struct utsname utsname;
	ElfHeader elf_header;
	char path[PATH_MAX];
	int status;

	if (tracee->exe == NULL)
		return;

	status = translate_path(tracee, path, AT_FDCWD, tracee->exe, false);
	if (status < 0)
		return;

	status = open_elf(path, &elf_header);
	if (status < 0)
		return;
	close(status);

	if (!IS_CLASS64(elf_header) || sizeof(word_t) == sizeof(uint64_t))
		return;

	notice(NULL, ERROR, USER,
		"'%s' is a 64-bit program whereas this version of "
		"%s handles 32-bit programs only", path, tracee->tool_name);

	status = uname(&utsname);
	if (status < 0)
		return;

	if (strcmp(utsname.machine, "x86_64") != 0)
		return;

	notice(NULL, INFO, USER,
		"use a 64-bit version of %s instead, it supports both 32 and 64-bit programs",
		tracee->tool_name);
}

/**
 * Wait then handle any event from any tracee.  This function returns
 * the exit status of the last terminated program.
 */
int event_loop()
{
	struct sigaction signal_action;
	int last_exit_status = -1;
	long status;
	int signum;

	/* Kill all tracees when exiting.  */
	status = atexit(kill_all_tracees);
	if (status != 0)
		notice(NULL, WARNING, INTERNAL, "atexit() failed");

	/* All signals are blocked when the signal handler is called.
	 * SIGINFO is used to know which process has signaled us and
	 * RESTART is used to restart waitpid(2) seamlessly.  */
	bzero(&signal_action, sizeof(signal_action));
	signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
	status = sigfillset(&signal_action.sa_mask);
	if (status < 0)
		notice(NULL, WARNING, SYSTEM, "sigfillset()");

	/* Handle all signals.  */
	for (signum = 0; signum < SIGRTMAX; signum++) {
		switch (signum) {
		case SIGQUIT:
		case SIGILL:
		case SIGABRT:
		case SIGFPE:
		case SIGSEGV:
			/* Kill all tracees on abnormal termination
			 * signals.  This ensures no process is left
			 * untraced.  */
			signal_action.sa_sigaction = kill_all_tracees2;
			break;

		case SIGUSR1:
		case SIGUSR2:
			/* Print on stderr the complete talloc
			 * hierarchy, useful for debug purpose.  */
			signal_action.sa_sigaction = print_talloc_hierarchy;
			break;

		case SIGCHLD:
		case SIGCONT:
		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			/* The default action is OK for these signals,
			 * they are related to tty and job control.  */
			continue;

		default:
			/* Ignore all other signals, including
			 * terminating ones (^C for instance). */
			signal_action.sa_sigaction = (void *)SIG_IGN;
			break;
		}

		status = sigaction(signum, &signal_action, NULL);
		if (status < 0 && errno != EINVAL)
			notice(NULL, WARNING, SYSTEM, "sigaction(%d)", signum);
	}

	while (1) {
		static bool seccomp_detected = false;
		int tracee_status;
		Tracee *tracee;
		pid_t pid;

		/* Not a signal-stop by default.  */
		int signal = 0;

		/* Wait for the next tracee's stop. */
		pid = waitpid(-1, &tracee_status, __WALL);
		if (pid < 0) {
			if (errno != ECHILD) {
				notice(NULL, ERROR, SYSTEM, "waitpid()");
				return EXIT_FAILURE;
			}
			break;
		}

		/* Get the information about this tracee. */
		tracee = get_tracee(NULL, pid, true);
		assert(tracee != NULL);

		/* When seccomp is enabled, all events are restarted
		 * in non-stop mode, but this default choice could be
		 * overwritten later if necessary.  The check against
		 * "sysexit_pending" ensures PTRACE_SYSCALL (used to
		 * hit the exit stage under seccomp) is not cleared
		 * due to an event that would happen before the exit
		 * stage, eg. PTRACE_EVENT_EXEC for the exit stage of
		 * execve(2).  */
		if (tracee->seccomp == ENABLED && !tracee->sysexit_pending)
			tracee->restart_how = PTRACE_CONT;
		else
			tracee->restart_how = PTRACE_SYSCALL;

		status = notify_extensions(tracee, NEW_STATUS, tracee_status, 0);
		if (status != 0)
			continue;

		if (WIFEXITED(tracee_status)) {
			last_exit_status = WEXITSTATUS(tracee_status);
			VERBOSE(tracee, 1, "pid %d: exited with status %d",
				pid, last_exit_status);
		}
		else if (WIFSIGNALED(tracee_status)) {
			check_architecture(tracee);
			VERBOSE(tracee, (int) (last_exit_status != -1),
				"pid %d: terminated with signal %d",
				pid, WTERMSIG(tracee_status));
		}
		else if (WIFSTOPPED(tracee_status)) {
			/* Don't use WSTOPSIG() to extract the signal
			 * since it clears the PTRACE_EVENT_* bits. */
			signal = (tracee_status & 0xfff00) >> 8;

			switch (signal) {
				static bool deliver_sigtrap = false;

			case SIGTRAP: {
				const unsigned long default_ptrace_options = (
					PTRACE_O_TRACESYSGOOD	|
					PTRACE_O_TRACEFORK	|
					PTRACE_O_TRACEVFORK	|
					PTRACE_O_TRACEVFORKDONE	|
					PTRACE_O_TRACEEXEC	|
					PTRACE_O_TRACECLONE	|
					PTRACE_O_TRACEEXIT);

				/* Distinguish some events from others and
				 * automatically trace each new process with
				 * the same options.
				 *
				 * Note that only the first bare SIGTRAP is
				 * related to the tracing loop, others SIGTRAP
				 * carry tracing information because of
				 * TRACE*FORK/CLONE/EXEC.  */
				if (deliver_sigtrap)
					break;  /* Deliver this signal as-is.  */

				deliver_sigtrap = true;

				/* Try to enable seccomp mode 2...  */
				status = ptrace(PTRACE_SETOPTIONS, tracee->pid, NULL,
						default_ptrace_options | PTRACE_O_TRACESECCOMP);
				if (status < 0) {
					/* ... otherwise use default options only.  */
					status = ptrace(PTRACE_SETOPTIONS, tracee->pid, NULL,
							default_ptrace_options);
					if (status < 0) {
						notice(tracee, ERROR, SYSTEM,
							"ptrace(PTRACE_SETOPTIONS)");
						return EXIT_FAILURE;
					}
				}
			}
				/* Fall through. */
			case SIGTRAP | 0x80:
				signal = 0;

				/* This tracee got signaled then freed during the
				   sysenter stage but the kernel reports the sysexit
				   stage; just discard this spurious tracee/event.  */
				if (tracee->exe == NULL) {
					ptrace(PTRACE_CONT, pid, 0, 0);
					TALLOC_FREE(tracee);
					continue;
				}

				switch (tracee->seccomp) {
				case ENABLED:
					if (tracee->status == 0) {
						/* sysenter: ensure the sysexit
						 * stage will be hit under seccomp.  */
						tracee->restart_how = PTRACE_SYSCALL;
						tracee->sysexit_pending = true;
					}
					else {
						/* sysexit: the next sysenter
						 * will be notified by seccomp.  */
						tracee->restart_how = PTRACE_CONT;
						tracee->sysexit_pending = false;
					}
					/* Fall through.  */
				case DISABLED:
					translate_syscall(tracee);

					/* This syscall has disabled seccomp.  */
					if (tracee->seccomp == DISABLING) {
						tracee->restart_how = PTRACE_SYSCALL;
						tracee->seccomp = DISABLED;
					}

					break;

				case DISABLING:
					/* Seccomp was disabled by the
					 * previous syscall, but its
					 * sysenter stage was already
					 * handled.  */
					tracee->seccomp = DISABLED;
					if (tracee->status == 0)
						tracee->status = 1;
					break;
				}
				break;

			case SIGTRAP | PTRACE_EVENT_SECCOMP2 << 8:
			case SIGTRAP | PTRACE_EVENT_SECCOMP << 8: {
				unsigned long flags = 0;

				signal = 0;

				if (!seccomp_detected) {
					VERBOSE(tracee, 1,
						"ptrace acceleration (seccomp mode 2) enabled");
					tracee->seccomp = ENABLED;
					seccomp_detected = true;
				}

				/* Use the common ptrace flow if
				 * seccomp was explicitely disabled
				 * for this tracee.  */
				if (tracee->seccomp != ENABLED)
					break;

				status = ptrace(PTRACE_GETEVENTMSG, tracee->pid, NULL, &flags);
				if (status < 0)
					break;

				/* Use the common ptrace flow when
				 * sysexit has to be handled.  */
				if ((flags & FILTER_SYSEXIT) != 0) {
					tracee->restart_how = PTRACE_SYSCALL;
					break;
				}

				/* Otherwise, handle the sysenter
				 * stage right now.  */
				tracee->restart_how = PTRACE_CONT;
				translate_syscall(tracee);

				/* This syscall has disabled seccomp, so
				 * move the ptrace flow back to the
				 * common path to ensure its sysexit
				 * will be handled.  */
				if (tracee->seccomp == DISABLING)
					tracee->restart_how = PTRACE_SYSCALL;
				break;
			}

			case SIGTRAP | PTRACE_EVENT_VFORK << 8:
				signal = 0;
				(void) new_child(tracee, CLONE_VFORK);
				break;

			case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			case SIGTRAP | PTRACE_EVENT_CLONE << 8:
				signal = 0;
				(void) new_child(tracee, 0);
				break;

			case SIGTRAP | PTRACE_EVENT_VFORK_DONE << 8:
			case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
			case SIGTRAP | PTRACE_EVENT_EXIT  << 8:
				signal = 0;
				break;

			case SIGSTOP:
				/* Stop this tracee until PRoot has received
				 * the EVENT_*FORK|CLONE notification.  */
				if (tracee->exe == NULL) {
					tracee->sigstop = SIGSTOP_PENDING;
					signal = -1;
				}

				/* For each tracee, the first SIGSTOP
				 * is only used to notify the tracer.  */
				if (tracee->sigstop == SIGSTOP_IGNORED) {
					tracee->sigstop = SIGSTOP_ALLOWED;
					signal = 0;
				}
				break;

			default:
				/* Deliver this signal as-is.  */
				break;
			}
		}

		/* Keep in the stopped state?.  */
		if (signal == -1)
			continue;

		/* Restart the tracee and stop it at the next entry or
		 * exit of a system call. */
		status = ptrace(tracee->restart_how, tracee->pid, NULL, signal);
		if (status < 0)
			TALLOC_FREE(tracee);
	}

	return last_exit_status;
}
