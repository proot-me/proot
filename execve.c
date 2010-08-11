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
 * Inspired by: execve(2) from the Linux kernel.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>        /* open(2), */
#include <unistd.h>       /* read(2), */
#include <sys/param.h>    /* ARG_MAX, */
#include <limits.h>       /* PATH_MAX, ARG_MAX, */
#include <errno.h>        /* ENAMETOOLONG, */
#include <sys/ptrace.h>   /* ptrace(2), */
#include <sys/user.h>     /* struct user*, */
#include <stdarg.h>       /* va_*(3), */
#include <string.h>       /* strlen(3), */
#include <alloca.h>       /* alloc(3), */
#include <stdio.h>        /* fprintf(3), */

#include "execve.h"
#include "arch.h"
#include "syscall.h"
#include "path.h"
#include "child_mem.h"

#ifndef ARG_MAX
#define ARG_MAX 131072
#endif

static const char *runner = NULL;

/**
 * XXX: TODO
 */
void init_module_execve(const char *runner_)
{
	if (runner_ != NULL)
		fprintf(stderr, "proot: option -r not yet supported.\n");
}

/**
 * Extract from the file @exec_file the script @interpreter and its
 * @argument if any. This function returns -errno on errno, 0 if
 * @exec_file is not a valid script, 1 if @interpreter is filled and 2
 * if both @interpreter and @argument are filled.
 *
 * Extract from "man 2 execve":
 *
 *     On Linux, the entire string following the interpreter name is
 *     passed as a *single* argument to the interpreter, and this
 *     string can include white space.
 */
static int get_interpreter(const char *exec_file, char interpreter[PATH_MAX], char argument[ARG_MAX])
{
	char tmp;
	int status;
	int fd;
	int i;

	interpreter[0] = '\0';
	argument[0] = '\0';

	/* Inspect the executable.  */
	fd = open(exec_file, O_RDONLY);
	if (fd < 0)
		return 0;

	status = read(fd, interpreter, 2 * sizeof(char));
	if (status != 2 * sizeof(char)) {
		status = 0;
		goto end;
	}

	/* Check if it really is a script text. */
	if (interpreter[0] != '#' || interpreter[1] != '!') {
		status = 0;
		goto end;
	}

	/* Skip leading spaces. */
	do {
		status = read(fd, &tmp, sizeof(char));
		if (status != sizeof(char)) {
			status = 0;
			goto end;
		}
	} while (tmp == ' ' || tmp == '\t');

	/* Slurp the interpreter path until the first space or end-of-line. */
	for (i = 0; i < PATH_MAX; i++) {
		switch (tmp) {
		case ' ':
		case '\t':
			/* Remove spaces in between the interpreter
			 * and the hypothetical argument. */
			interpreter[i] = '\0';
			break;

		case '\n':
		case '\r':
			/* There is no argument nor trailing whitespaces. */
			interpreter[i] = '\0';
			status = 1;
			goto end;

		default:
			/* There is an argument if the previous
			 * character was '\0'. */
			if (i > 1 && interpreter[i - 1] == '\0')
				goto argument;
			else
				interpreter[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status != sizeof(char)) {
			status = 0;
			goto end;
		}
	}

	/* The interpreter path is too long. */
	status = -ENAMETOOLONG;
	goto end;

argument:
	/* Slurp the argument until the end-of-line. */
	for (i = 0; i < ARG_MAX; i++) {
		switch (tmp) {
		case '\n':
		case '\r':
			argument[i] = '\0';

			/* Remove trailing spaces. */
			for (i--; i > 0 && (argument[i] == ' ' || argument[i] == '\t'); i--)
				argument[i] = '\0';

			status = 2;
			goto end;

		default:
			argument[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status != sizeof(char)) {
			status = 0;
			goto end;
		}
	}

	/* The argument is too long, just ignore it. */
	argument[0] = '\0';
	status = 1;

end:
	close(fd);
	return status;
}

/**
 * XXX: TODO
 */
static int check_elf_interpreter(const char *exec_file)
{
	return 0;
}

/**
 * Replace the argv[0] of an execve() syscall made by the child
 * process @pid with the C strings specified in the variable parameter
 * lists (@number_args elements).
 *
 *    new_argv[] = { &new_argv0, ..., &new_argvN, &old_argv[1], ... }
 *
 * Technically, we use the memory below the stack pointer to store the
 * new arguments and the new array of pointers to these arguments:
 *
 *      new_argv[]                            new_argv1         new_argv0
 *     /                                             \                  \
 *    | &new_argv0 | &new_argv1 | &old_argv[1] | ... | "/bin/script.sh" | "/bin/sh" |
 *                                                                                  /
 *                                                                     stack pointer
 */
static int prepend_args(pid_t pid, int number_args, ...)
{
	word_t *new_args = alloca(number_args * sizeof(word_t));

	word_t old_child_argv;
	word_t new_child_argv;
	word_t argp;

	va_list args;

	size_t size;
	long status;
	int i;

	/* Copy the new arguments in the child's stack. */
	argp = (word_t) ptrace(PTRACE_PEEKUSER, pid, USER_REGS_OFFSET(REG_SP), NULL);
	if (errno != 0)
		return -EFAULT;

	va_start(args, number_args);
	for (i = 0; i < number_args; i++) {
		const char *arg = va_arg(args, const char *);
		if (arg == NULL) {
			new_args[i] = 0;
			continue;
		}

		size = strlen(arg) + 1;
		argp -= size;

		status = copy_to_child(pid, argp, arg, size);
		if (status < 0) {
			va_end(args);
			return status;
		}

		new_args[i] = argp;
	}
	va_end(args);

	new_child_argv = argp;

	/* Search for the end of argv[], that is, the terminating NULL pointer. */
	old_child_argv = get_sysarg(pid, SYSARG_2);

	for (i = 0; ; i++) {
		argp = (word_t) ptrace(PTRACE_PEEKDATA, pid, old_child_argv + i * sizeof(word_t), NULL);
		if (errno != 0)
			return -EFAULT;

		if (argp == 0)
			break;
	}

	/* Copy the pointers to the old arguments backward in the stack, except the first one. */
	for (; i > 0; i--) {
		argp = (word_t) ptrace(PTRACE_PEEKDATA, pid, old_child_argv + i * sizeof(word_t), NULL);
		if (errno != 0)
			return -EFAULT;

		new_child_argv -= sizeof(word_t);

		status = ptrace(PTRACE_POKEDATA, pid, new_child_argv, argp);
		if (status <0)
			return -EFAULT;
	}

	/* Copy the pointers to the new arguments backward in the stack. */
	for (i = number_args - 1; i >= 0; i--) {
		if (new_args[i] == 0)
			continue;

		new_child_argv -= sizeof(word_t);

		status = ptrace(PTRACE_POKEDATA, pid, new_child_argv, new_args[i]);
		if (status <0)
			return -EFAULT;
	}

	/* Update the pointer to the new argv[]. */
	set_sysarg(pid, SYSARG_2, new_child_argv);

	/* Update the stack pointer to ensure [internal] coherency. It prevents
	 * memory corruption if functions like set_sysarg_path() are called later. */
	status = ptrace(PTRACE_POKEUSER, pid, USER_REGS_OFFSET(REG_SP), new_child_argv);
	if (status < 0)
		return -EFAULT;

	return 0;
}

/**
 * Translate the arguments of the execve() syscall made by the child
 * process @pid. This syscall needs a very special treatment for
 * script files because according to "man 2 execve":
 *
 *     An interpreter script is a text file [...] whose first line is
 *     of the form:
 *
 *         #! interpreter [optional-arg]
 *
 *     The interpreter must be a valid pathname for an executable
 *     which is not itself a script.  If the filename argument of
 *     execve() specifies an interpreter script, then interpreter will
 *     be invoked with the following arguments:
 *
 *         interpreter [optional-arg] filename arg...
 *
 *     where arg...  is the series of words pointed to by the argv
 *     argument of execve().
 *
 * Let's take the following example:
 *
 *     execve("/bin/script.sh", argv = [ "script.sh", "arg1", arg2", ... ], envp);
 *
 * We can not just translate the first parameter because the kernel
 * will actually run the interpreter "/bin/sh" with the translated
 * path to the script file "/tmp/new_root/bin/script.sh" as its first
 * argument. Technically, we want the opposite behaviour, that is, we
 * want to run the translated path to the interpreter
 * "/tmp/new_root/bin/sh" with the non translated path to the script
 * "/bin/script.sh" as its first parameter (will be translated later):
 *
 *     execve("/tmp/new_root/bin/sh", argv = [ "/bin/sh", "/bin/script.sh", "arg1", arg2", ... ], envp);
 */
int translate_execve(pid_t pid)
{
	int status;

	char old_path[PATH_MAX];
	char new_path[PATH_MAX];

	char interpreter[PATH_MAX];
	char argument[ARG_MAX];
	char *optional_arg;

	status = get_sysarg_path(pid, old_path, SYSARG_1);
	if (status < 0)
		return status;

	status = translate_path(pid, new_path, old_path, REGULAR);
	if (status < 0)
		return status;

	status = get_interpreter(new_path, interpreter, argument);
	optional_arg = argument;

	switch (status) {
	/* Not a script. */
	case 0:
		break;

	/* Require an interpreter without argument. */
	case 1:
		optional_arg = NULL;

	/* Require an interpreter with an argument. */
	case 2:
		/* Pass to the interpreter the "fake" path to the script. */
		detranslate_path(new_path, 1);
		if (status < 0)
			return status;

		/* argv[] = { &interpreter, &arg (optional), &script, &old_argv[1], &old_argv[2], ... } */
		status = prepend_args(pid, 4, runner, interpreter, optional_arg, new_path);
		if (status < 0)
			return status;

		status = translate_path(pid, new_path, interpreter, REGULAR);
		if (status < 0)
			return status;
		break;

	default:
		return status;
	}

	/* Ensure someone is not using a nasty ELF interpreter. */
	status = check_elf_interpreter(new_path);
	if (status < 0)
		return status;

	set_sysarg_path(pid, new_path, SYSARG_1);

	return 0;
}
