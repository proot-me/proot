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
 */

#include <stdlib.h>     /* exit(3), EXIT_*, */
#include <stdio.h>      /* puts(3), */
#include <limits.h>     /* PATH_MAX, */
#include <string.h>     /* strcmp(3), */

#include "trace.h"
#include "path/path.h"
#include "path/binding.h"
#include "syscall/syscall.h"
#include "execve/execve.h"
#include "notice.h"
#include "config.h"

#include "compat.h"

static const char *opt_new_root = NULL;
static char *opt_args_default[] = { "/bin/sh", NULL };
static char **opt_args = &opt_args_default[0];

static const char *opt_pwd = NULL;
static const char *opt_runner = NULL;

static int opt_no_elf_interp = 0;
static int opt_many_jobs = 0;
static bool opt_allow_unknown = false;
static bool opt_allow_ptrace = false;
static bool opt_fake_id0 = false;
static int opt_disable_aslr = 0;
static const char *opt_kernel_release = NULL;

static int opt_qemu = 0;
static int opt_check_fd = 0;
static int opt_check_syscall = 0;

#define xstr(s) str(s)
#define str(s) #s

static void exit_usage(void)
{
	puts("");
	puts("PRoot-" xstr(VERSION) ": isolate programs in a guest root file-system.");
	puts("");
	puts("Usages:");
	puts("  proot [options] /path/to/guest/rootfs");
	puts("  proot [options] /path/to/guest/rootfs program [options]");
	puts("");
	puts("Main options:");
	puts("  -b <p1>[:p2]  bind host path <p1> in the guest rootfs (to <p2> if specified)");
/*	puts("  -r <runner>   XXX */
	puts("  -q <qemu>     use <qemu> to handle each new process");
	puts("  -w <path>     set the working directory to <path> (default is \"/\")");
	puts("");
	puts("Alias options:");
	puts("  -B            alias for: -b $HOME -b /tmp -b /dev -b /proc -b /sys");
	puts("                           -b /etc/passwd -b /etc/group -b /etc/localtime");
	puts("                           -b /etc/nsswitch.conf -b /etc/resolv.conf");
/*	puts("  -R <runner>   XXX */
	puts("  -Q <qemu>     alias for: -q <qemu> -B -a");
	puts("  -W            alias for: -w $PWD -b $PWD");
	puts("");
	puts("Misc. options:");
	puts("  -v            increase the verbose level");
	puts("  -V            print version/copyright/license/contact then exit");
/*	puts("  -D <X>=<Y>    set the environment variable <X> to <Y>"); */
/*	puts("  -U <X>        deletes the variable <X> from the environment"); */
	puts("  -e            don't use the ELF interpreter (typically \"ld.so\") as loader");
	puts("  -u            don't block unknown syscalls");
	puts("  -p            don't block ptrace(2)");
	puts("  -a            disable randomization of the virtual address space (ASLR)");
	puts("  -k <string>   make uname(2) report <string> as the kernel release");
	puts("  -0            make get*[gu]id report the user/group identity 0 (\"root\" id)");
/*	puts("  -f <file>     read the configuration for \"binfmt_misc\" support from <file>"); */
/*	puts("  -j <integer>  use <integer> jobs (faster but prone to race condition exploit)"); */
/*	puts("  -d            check every /proc/$pid/fd/\* point to a translated path (slow!)"); */
/*	puts("  -s            check /proc/$pid/syscall agrees with the internal state"); */
	puts("");
	puts("Examples:");
	puts("  proot -B /media/slackware-13.37/");
	puts("  proot -Q qemu-arm /media/armedslack-13.37/ /usr/bin/emacs");
	puts("");
	puts("Notes:");
	puts("  * \"/bin/sh\" is launched by default if no program were specified.");
	puts("  * you can pass options to QEMU directly by specifying a");
	puts("    comma-separated list, for instance \"-Q qemu-arm,-g,1234\".");
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

		case 'm':
			notice(WARNING, USER, "option -m is deprecated, use -b instead");
			/* fall through. */
		case 'b':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -b/-m");
			ptr = strchr(argv[i], ':');
			if (ptr != NULL) {
				*ptr = '\0';
				ptr++;
			}
			bind_path(argv[i], ptr);
			break;

		case 'v':
			verbose_level++;
			break;

		case 'V':
			puts("PRoot version " xstr(VERSION));
			puts("");
			puts("Contact cedric.vincent@gmail.com for bug reports, suggestions, ...");
			puts("Copyright (C) 2010, 2011 STMicroelectronics, licensed under GPL v2 or later.");
			exit(EXIT_FAILURE);
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
			opt_disable_aslr = 1;
			/* XXX LD_PRELOAD=libgcc_s.so */
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
				bind_path(opt_pwd, NULL);
			break;

		case 'Q':
			opt_qemu = 1;
			opt_disable_aslr = 1;
			/* XXX LD_PRELOAD=libgcc_s.so */
			/* fall through. */
		case 'R':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -R/-Q");
			opt_runner = argv[i];
			/* fall through. */
		case 'M':
			if (argv[i][1] == 'M')
				notice(WARNING, USER, "option -M is deprecated, use -B instead");
			/* fall through. */
		case 'B':
			/* Usefull for the glibc, generally only one access. */
			bind_path("/etc/host.conf", NULL);
			bind_path("/etc/hosts", NULL);
			bind_path("/etc/hosts.equiv", NULL);
			bind_path("/etc/mtab", NULL);
			bind_path("/etc/netgroup", NULL);
			bind_path("/etc/networks", NULL);
			bind_path("/etc/passwd", NULL);
			bind_path("/etc/group", NULL);
			bind_path("/etc/nsswitch.conf", NULL);
			bind_path("/etc/resolv.conf", NULL);

			/* Path with dynamic content. */
			bind_path("/dev", NULL);
			bind_path("/sys", NULL);
			bind_path("/proc", NULL);

			/* Commonly accessed paths. */
			bind_path("/tmp", NULL);
			bind_path("/etc/localtime", NULL);
			if (getenv("HOME") != NULL)
				bind_path(getenv("HOME"), NULL);
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

		case 'a':
			opt_disable_aslr = 1;
			break;

		case 'k':
			i++;
			if (i >= argc)
				notice(ERROR, USER, "missing value for the option -k");
			opt_kernel_release = argv[i];
			break;

		case '0':
			opt_fake_id0 = true;
			break;

		case 'd':
			opt_check_fd = 1;
			break;

		case 's':
			opt_check_syscall = 1;
			break;

		case 'h':
			exit_usage();
			break;

		default:
			notice(ERROR, USER, "unknown option '-%c', try '-h' for help.", argv[i][1]);
			break;
		}
	}

	if (argc == i)
		exit_usage();

	opt_new_root = argv[i];

	if (argc - i == 1) {
		opt_args_default[0] = "/bin/sh";
	}
	else  {
		opt_args = &argv[i + 1];
	}

	pid = atoi(opt_args[0]);
	if (opt_args[1] != NULL)
		pid = 0;

	init_module_path(opt_new_root, opt_runner != NULL);
	init_module_tracee_info();
	init_module_syscall(opt_check_syscall, opt_allow_unknown, opt_allow_ptrace, opt_fake_id0, opt_kernel_release);
	init_module_execve(opt_runner, opt_qemu, opt_no_elf_interp);

	return pid; /* XXX: pid attachement not yet [officially] supported. */
}

static void print_execve_help(char *argv0)
{
	notice(WARNING, SYSTEM, "execv(\"%s\")", argv0);

	notice(INFO, USER, "\
Possible causes:");
	notice(INFO, USER, "\
  * <program> was not found ($PATH not yet supported);");
	notice(INFO, USER, "\
  * <program> is a script but its interpreter (ex. /bin/sh) was not found;");
	if (opt_runner || opt_qemu) {
		notice(INFO, USER, "\
  * <qemu> does not work correctly.");
	}
	else {
		notice(INFO, USER, "\
  * <program> is an ELF but its interpreter (ex. ld-linux.so) was not found;");
		notice(INFO, USER, "\
  * <program> is a foreign binary but no <qemu> was specified;");
	}
}

int main(int argc, char *argv[])
{
	pid_t pid;
	bool status;

	pid = parse_options(argc, argv);
	status = pid ? attach_process(pid) : launch_process(opt_new_root, opt_pwd, opt_args, opt_disable_aslr);
	if (!status) {
		print_execve_help(opt_args[0]);
		exit(EXIT_FAILURE);
	}

	return event_loop(opt_check_fd);
}
