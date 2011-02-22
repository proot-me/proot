/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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

#include <unistd.h>     /* fork(2), chdir(2), exec*(3), readlink(2), */
#include <stdlib.h>     /* exit(3), EXIT_*, */
#include <stdio.h>      /* puts(3), */
#include <sys/wait.h>   /* wait(2), */
#include <sys/ptrace.h> /* ptrace(2), */
#include <limits.h>     /* PATH_MAX, */
#include <string.h>     /* strcmp(3), */
#include <errno.h>      /* errno(3), */
#include <libgen.h>     /* basename(3), */

#include "path.h"
#include "child_info.h"
#include "syscall.h" /* translate_syscall(), */
#include "execve.h"
#include "notice.h"

#include "compat.h"

static const char *opt_new_root = NULL;
static char *opt_args_default[] = { "/bin/sh", NULL };
static char **opt_args = &opt_args_default[0];

static const char *opt_pwd = NULL;
static const char *opt_runner = NULL;

static int opt_no_elf_interp = 0;
static int opt_many_jobs = 0;
static int opt_allow_unknown = 0;
static int opt_allow_ptrace = 0;

static int opt_qemu = 0;
static int opt_check_fd = 0;
static int opt_check_syscall = 0;

#define xstr(s) str(s)
#define str(s) #s

static void exit_usage(void)
{
	puts("");
	puts("PRoot-" xstr(VERSION) ": relocate programs to an alternate root file-system, à la chroot.");
	puts("");
	puts("Usages:");
	puts("  proot [options] /path/to/alternate/rootfs");
	puts("  proot [options] /path/to/alternate/rootfs program [options]");
	puts("");
	puts("Main options:");
	puts("  -m <p1>[:p2]  mirror path <p1> from the real rootfs into the alternate rootfs");
	puts("                at location <p2> if specified, otherwise at the same location.");
/*	puts("  -r <runner>   XXX */
	puts("  -q <qemu>     use <qemu> to handle each process; you can pass options by");
	puts("                using a comma-separated list, for instance \"qemu-sh4,-g,1234\"");
	puts("  -e            don't use the ELF interpreter (typically \"ld.so\") as loader");
	puts("  -w <path>     set the working directory to <path> (default is \"/\")");
	puts("");
	puts("Alias options:");
	puts("  -M            alias for: -m $HOME -m /dev -m /proc -m /sys -m /etc/passwd");
	puts("                           -m /etc/group -m /etc/localtime -m /etc/nsswitch.conf");
	puts("                           -m /etc/resolv.conf");
/*	puts("  -R <runner>   XXX */
	puts("  -Q <qemu>     alias for: -q <qemu> -M");
	puts("  -W            alias for: -w $PWD -m $PWD");
	puts("");
	puts("Misc. options:");
	puts("  -v            increase the verbose level");
/*	puts("  -D <X>=<Y>    set the environment variable <X> to <Y>"); */
/*	puts("  -U <X>        deletes the variable <X> from the environment"); */
	puts("  -u            don't block unknown syscalls");
	puts("  -p            don't block ptrace(2)");
/*	puts("  -f <file>     read the configuration for \"binfmt_misc\" support from <file>"); */
/*	puts("  -j <integer>  use <integer> jobs (faster but prone to race condition exploit)"); */
/*	puts("  -d            check every /proc/$pid/fd/\* point to a translated path (slow!)"); */
/*	puts("  -s            check /proc/$pid/syscall agrees with the internal state"); */
	puts("");
	puts("Examples:");
	puts("  proot -M /media/slackware64-13.1/");
	puts("  proot -Q qemu-arm /media/armedslack-12.2/ /usr/bin/emacs");
	puts("");
	puts("Contact cedric.vincent@gmail.com for bug reports, suggestions, ...");
	puts("Copyright (C) 2010, 2011 STMicroelectronics, licensed under GPL v2 or later.");
	puts("");

	exit(EXIT_FAILURE);
}

static pid_t parse_options(int argc, char *argv[])
{
	int status;
	char *ptr;
	pid_t pid;
	int i;

	/* Stupid command-line parser. */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			break; /* End of PRoot options. */

		if (argv[i][2] != '\0') {
			exit_usage();
			exit(EXIT_FAILURE);			
		}

		switch (argv[i][1]) {
		case 'w':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -w");
			opt_pwd = argv[i];
			break;

		case 'x':
			notice(WARNING, USER, "option -x is deprecated, use -m instead");
			/* fall through. */
		case 'm':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -m");
			ptr = strchr(argv[i], ':');
			if (ptr != NULL) {
				*ptr = '\0';
				ptr++;
			}
			mirror_path(argv[i], ptr);
			break;

		case 'v':
			verbose_level++;
			break;

		case 'D':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -D");
			ptr = strchr(argv[i], '=');
			if (ptr != NULL) {
				*ptr = '\0';
				ptr++;
			}
			else
				ptr = argv[i] + strlen(argv[i]);

			status = setenv(argv[i], ptr, 1);
			if (status < 0)
				notice(WARNING, SYSTEM, "setenv(\"%s\", \"%s\")", argv[i], ptr);
			break;

		case 'U':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -U");
			status = unsetenv(argv[i]);
			if (status < 0)
				notice(WARNING, SYSTEM, "unsetenv(\"%s\")", argv[i]);
			break;

		case 'e':
			opt_no_elf_interp = 1;
			break;

		case 'q':
			opt_qemu = 1;
			/* fall through. */
		case 'r':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -r/-q");
			opt_runner = argv[i];
			break;

		case 'W':
			opt_pwd = getenv("PWD");
			if (opt_pwd == NULL)
				notice(WARNING, SYSTEM, "getenv(\"PWD\")");
			else
				mirror_path(opt_pwd, NULL);
			break;

		case 'Q':
			opt_qemu = 1;
			/* fall through. */
		case 'R':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -R/-Q");
			opt_runner = argv[i];
			/* fall through. */
		case 'X':
			if (argv[i][1] == 'X')
				notice(WARNING, USER, "option -X is deprecated, use -M instead");
			/* fall through. */
		case 'M':
			/* Usefull for the glibc, generally only one access. */
			mirror_path("/etc/passwd", NULL);
			mirror_path("/etc/group", NULL);
			mirror_path("/etc/nsswitch.conf", NULL);
			mirror_path("/etc/resolv.conf", NULL);

			/* Path with dynamic content. */
			mirror_path("/dev", NULL);
			mirror_path("/sys", NULL);
			mirror_path("/proc", NULL);

			/* Commonly accssed paths. */
			mirror_path("/etc/localtime", NULL);
			if (getenv("HOME") != NULL)
				mirror_path(getenv("HOME"), NULL);
			break;

		case 'j':
			notice(WARNING, USER, "option -j not yet supported");
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -j");
			opt_many_jobs = atoi(argv[i]);
			if (opt_many_jobs == 0)
				notice(ERROR, USER, "-j \"%s\" is not valid", argv[i]);
			break;

		case 'u':
			opt_allow_unknown = 1;
			break;

		case 'p':
			opt_allow_ptrace = 1;
			break;

		case 'd':
			opt_check_fd = 1;
			break;

		case 's':
			opt_check_syscall = 1;
			break;

		default:
			exit_usage();
		}
	}

	if (argc == i)
		exit_usage();

	opt_new_root = argv[i];

	if (argc - i == 1) {
		char *shell = getenv("SHELL");
		if (shell != NULL)
			opt_args_default[0] = shell;
	}
	else  {
		opt_args = &argv[i + 1];
	}

	pid = atoi(opt_args[0]);
	if (opt_args[1] != NULL)
		pid = 0;

	init_module_path(opt_new_root, opt_runner != NULL);
	init_module_child_info();
	init_module_syscall(opt_check_syscall, opt_allow_unknown, opt_allow_ptrace);
	init_module_execve(opt_runner, opt_qemu, opt_no_elf_interp);

	return pid; /* XXX: pid attachement not yet [officially] supported. */
}

static void launch_process()
{
	char launcher[PATH_MAX];
	char new_root[PATH_MAX];
	char pwd[PATH_MAX];
	long status;
	pid_t pid;
	int i;

	pid = fork();
	switch(pid) {
	case -1:
		notice(ERROR, SYSTEM, "fork()");

	case 0: /* child */

		if (realpath("/proc/self/exe", launcher) == NULL)
			notice(ERROR, SYSTEM, "realpath(\"/proc/self/exe\")");

		/* Declare myself as ptraceable before executing the
		 * requested program. */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0)
			notice(ERROR, SYSTEM, "ptrace(TRACEME)");

		if (realpath(opt_new_root, new_root) == NULL)
			notice(ERROR, SYSTEM, "realpath(\"%s\")", opt_new_root);

		/* Ensure the child starts in a valid cwd within the
		 * new root. */
		status = chdir(new_root);
		if (status < 0)
			notice(ERROR, SYSTEM, "chdir(\"%s\")", new_root);

		if (opt_pwd != NULL) {
			status = translate_path(NULL, pwd, AT_FDCWD, opt_pwd, 1);
			if (status < 0) {
				if (errno == 0)
					errno = -status;
				notice(WARNING, SYSTEM, "translate_path(\"%s\")", opt_pwd);
				goto default_pwd;
			}

			status = chdir(pwd);
			if (status < 0) {
				notice(WARNING, SYSTEM, "chdir(\"%s\")", pwd);
				goto default_pwd;
			}

			status = detranslate_path(pwd, 1);
			if (status < 0) {
				notice(WARNING, SYSTEM, "detranslate_path(\"%s\")", pwd);
				goto default_pwd;
			}
		}
		else {
		default_pwd:
			strcpy(pwd, "/");
		}

		status = setenv("PWD", pwd, 1);
		if (status < 0)
			notice(ERROR, SYSTEM, "setenv(\"PWD\", \"%s\")", pwd);

		status = setenv("OLDPWD", pwd, 1);
		if (status < 0)
			notice(ERROR, SYSTEM, "setenv(\"OLDPWD\"), \"%s\"", pwd);

		/* Warn about open file descriptors. They won't be
		 * translated until they are closed. */
		list_open_fd(getpid());

		if (verbose_level >= 1) {
			fprintf(stderr, "proot:");
			for (i = 0; opt_args[i] != NULL; i++)
				fprintf(stderr, " %s", opt_args[i]);
			fprintf(stderr, "\n");
		}

		status = execvp(launcher, opt_args);
		notice(ERROR, SYSTEM, "execvp(\"%s\")", launcher);

	default: /* parent */
		if (new_child(pid) == NULL)
			exit(EXIT_FAILURE);
		break;
	}
}

static void attach_process(pid_t pid)
{
	long status;
	struct child_info *child;

	notice(WARNING, USER, "attaching a process on-the-fly is still experimental");

	status = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	if (status < 0)
		notice(ERROR, SYSTEM, "ptrace(ATTACH, %d)", pid);

	/* Warn about open file descriptors. They won't be translated
	 * until they are closed. */
	list_open_fd(pid);

	child = new_child(pid);
	if (child == NULL)
		exit(EXIT_FAILURE);
}

static int event_loop()
{
	int last_exit_status = -1;
	int child_status;
	long status;
	int signal;
	pid_t pid;

	signal = 0;
	while (get_nb_children() > 0) {
		/* Wait for the next child's stop. */
		pid = waitpid(-1, &child_status, __WALL);
		if (pid < 0)
			notice(ERROR, SYSTEM, "waitpid()");

		/* Check every child file descriptors. */
		if (opt_check_fd != 0)
			foreach_child(check_fd);

		if (WIFEXITED(child_status)) {
			VERBOSE(1, "pid %d: exited with status %d",
			           pid, WEXITSTATUS(child_status));
			last_exit_status = WEXITSTATUS(child_status);
			delete_child(pid);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFSIGNALED(child_status)) {
			VERBOSE(get_nb_children() != 1,
				"pid %d: terminated with signal %d",
				pid, WTERMSIG(child_status));
			delete_child(pid);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFCONTINUED(child_status)) {
			VERBOSE(1, "pid %d: continued", pid);
			signal = SIGCONT;
		}
		else if (WIFSTOPPED(child_status)) {

			/* Don't WSTOPSIG() to extract the signal
			 * since it clears the PTRACE_EVENT_* bits. */
			signal = (child_status & 0xfff00) >> 8;

			switch (signal) {
			case SIGTRAP | 0x80:
				status = translate_syscall(pid);
				if (status < 0) {
					/* The process died in a syscall. */
					delete_child(pid);
					continue; /* Skip the call to ptrace(SYSCALL). */
				}
				signal = 0;
				break;

			case SIGTRAP:
				/* Distinguish some events from others and
				 * automatically trace each new process with
				 * the same options.  Note: only the first
				 * process should come here (because of
				 * TRACE*FORK/CLONE). */
				status = ptrace(PTRACE_SETOPTIONS, pid, NULL,
						PTRACE_O_TRACESYSGOOD |
						PTRACE_O_TRACEFORK    |
						PTRACE_O_TRACEVFORK   |
						PTRACE_O_TRACECLONE);
				if (status < 0)
					notice(ERROR, SYSTEM, "ptrace(PTRACE_SETOPTIONS)");

				signal = 0;
				break;

			case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			case SIGTRAP | PTRACE_EVENT_CLONE << 8:
				/* Ignore these signals. */
				signal = 0;
				break;

			default:
				/* Propagate all other signals. */
				break;
			}
		}
		else {
			notice(WARNING, INTERNAL, "unknown trace event");
			signal = 0;
		}

		/* Restart the child and stop it at the next entry or exit of
		 * a system call. */
		status = ptrace(PTRACE_SYSCALL, pid, NULL, signal);
		if (status < 0)
			notice(ERROR, SYSTEM, "ptrace(SYSCALL)");
	}

	return last_exit_status;
}

int main(int argc, char *argv[])
{
	pid_t pid;
	ssize_t status;
	char argv0[PATH_MAX];
	char self[PATH_MAX];
	char *basename_argv0;
	char *basename_self;

	strcpy(argv0, argv[0]);
	basename_argv0 = basename(argv0);

	status = readlink("/proc/self/exe", self, sizeof(self));
	if (status < 0) {
		notice(WARNING, SYSTEM, "readlink(\"/proc/self/exe\")");
		/* Fallback to "proot" if "/proc" is not mirrored. */
		strcpy(self, "proot");
		status = sizeof("proot");
	}
	if (status == sizeof(self))
		notice(ERROR, INTERNAL, "readlink(\"/proc/self/exe\") too long");
	self[status] = '\0';
	basename_self = basename(self);

	/* Use myself as a launcher. We need a launcher to
	 * ensure the first execve(2) is catched by PRoot. */
	if (strcmp(basename_argv0, basename_self) != 0) {
		execv(argv[0], argv);
		notice(WARNING, SYSTEM, "execv(\"%s\")", argv[0]);
		notice(INFO, USER, "Possible causes:\n"
		       "  * <program> was not found ($PATH not yet supported);\n"
		       "  * <program> is a script but its interpreter (/bin/sh for instance) was not found;\n"
		       "  * <program> is an ELF but its interpreter (/lib/ld-linux.so typically) was not found; or\n"
		       "  * <qemu> (if specified) does not work correctly.\n"
		       "Try to run with -v to get a better diagnostic.");
		exit(EXIT_FAILURE);
	}

	pid = parse_options(argc, argv);

	if (pid != 0)
		attach_process(pid);
	else
		launch_process();

	return event_loop();
}
