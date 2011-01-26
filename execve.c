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

#include <unistd.h>       /* access(2), lstat(2), */
#include <sys/stat.h>     /* lstat(2), S_ISREG(), */
#include <limits.h>       /* PATH_MAX, ARG_MAX, */
#include <errno.h>        /* ENAMETOOLONG, */
#include <sys/ptrace.h>   /* ptrace(2), */
#include <sys/user.h>     /* struct user*, */
#include <stdarg.h>       /* va_*(3), */
#include <string.h>       /* strlen(3), */
#include <stdlib.h>       /* realpath(3), exit(3), EXIT_*, */
#include <assert.h>       /* assert(3), */

#include "execve.h"
#include "arch.h"
#include "syscall.h"
#include "path.h"
#include "child_mem.h"
#include "notice.h"
#include "ureg.h"
#include "interp.h"

#include "compat.h"

static char runner[PATH_MAX] = { '\0', };
static char runner_args[ARG_MAX] = { '\0', };
static int nb_runner_args;
static int skip_elf_interp = 0;
static int runner_is_qemu = 0;

/**
 * Initialize internal data of the execve module.
 */
void init_module_execve(const char *opt_runner, int opt_qemu, int no_elf_interp)
{
	int status;
	char *tmp, *tmp2, *tmp3;

	skip_elf_interp = no_elf_interp;

	if (opt_runner == NULL)
		return;

	runner_is_qemu = opt_qemu;

	/* Split apart the name of the runner and its options, if any. */
	tmp = index(opt_runner, ',');
	if (tmp != NULL) {
		*tmp = '\0';
		tmp++;

		if (strlen(tmp) >= ARG_MAX)
			notice(ERROR, USER, "the option \"-q %s\" is too long", opt_runner);

		strcpy(runner_args, tmp);
		for (nb_runner_args = 1; (tmp = index(tmp, ',')) != NULL; nb_runner_args++)
			tmp++;
	}

	/* Search for the runner in $PATH only if it is not a relative
	 * not an absolute path. */
	tmp = getenv("PATH");
	tmp2 = tmp;
	if (tmp != NULL
	    && strncmp(opt_runner, "/", 1)   != 0
	    && strncmp(opt_runner, "./", 2)  != 0
	    && strncmp(opt_runner, "../", 3) != 0) {

		/* Iterate over each entry of $PATH. */
		do {
			ptrdiff_t length;

			tmp = index(tmp, ':');
			if (tmp == NULL)
				length = strlen(tmp2);
			else {
				length = tmp - tmp2;
				tmp++;
			}

			/* Ensure there is enough room in runner[]. */
			if (length + strlen(opt_runner) + 2 >= PATH_MAX) {
				notice(WARNING, USER, "component \"%s\" from $PATH is too long", tmp2);
				goto next;
			}
			strncpy(runner, tmp2, length);
			runner[length] = '\0';
			strcat(runner, "/");
			strcat(runner, opt_runner);

			/* Canonicalize the path to runner. */
			tmp3 = realpath(runner, NULL);
			if (tmp3 == NULL)
				goto next;

			/* Check if the runner is executable. */
			status = access(tmp3, X_OK);
			if (status >= 0) {
				/* length(tmp3) < PATH_MAX according to realpath(3) */
				strcpy(runner, tmp3);
				VERBOSE(1, "runner is %s", runner);

				free(tmp3);
				return;
			}
			free(tmp3);

		next:
			tmp2 = tmp;
		} while (tmp != NULL);

		/* Otherwise check in the current directory. */
	}

	/* Canonicalize the path to the runner. */
	if (realpath(opt_runner, runner) == NULL)
		notice(ERROR, SYSTEM, "realpath(\"%s\")", opt_runner);

	/* Ensure the runner is executable. */
	status = access(runner, X_OK);
	if (status < 0)
		notice(ERROR, SYSTEM, "access(\"%s\", X_OK)", runner);
}

/**
 * Substitute (and free) the first entry of *@argv with the C strings
 * specified in the variable parameter lists (@nb_new_args elements).
 * This function returns -errno if an error occured, otherwise 0.
 *
 *  Technically:
 *
 *    | argv[0] | argv[1] | ... | argv[n] |
 *
 *  becomes:
 *
 *    | new_argv[0] | ... | new_argv[nb - 1] | argv[1] | ... | argv[n] |
 */
int substitute_argv0(char **argv[], int nb_new_args, ...)
{
	va_list args;
	void *tmp;
	int i;

	for (i = 0; (*argv)[i] != NULL; i++)
		;
	assert(i > 0);

	/* Don't kill *argv if the reallocation failed, all entries
	 * will be freed in translate_execve(). */
	tmp = realloc(*argv, (nb_new_args + i) * sizeof(char *));
	if (tmp == NULL)
		return -ENOMEM;
	*argv = tmp;

	free(*argv[0]);
	*argv[0] = NULL;

	/* Move the old entries to let space at the beginning for the
	 * new ones. */
	memmove(*argv + nb_new_args, *argv + 1, i * sizeof(char *));

	/* Each new entries will be allocated into the heap since we
	 * don't rely on the liveness of the input parameters. */
	va_start(args, nb_new_args);
	for (i = 0; i < nb_new_args; i++) {
		char *new_arg = va_arg(args, char *);
		(*argv)[i] = calloc(strlen(new_arg) + 1, sizeof(char));
		if ((*argv)[i] == NULL) {
			va_end(args);
			return -ENOMEM; /* XXX: *argv is inconsistent! */
		}
		strcpy((*argv)[i], new_arg);
	}
	va_end(args);

	return 0;
}

/**
 * Insert each entry of runner_args[] in between the first entry of
 * *@argv and its sucessor.  This function returns -errno if an error
 * occured, otherwise 0.
 *
 *  Technically:
 *
 *    | argv[0] | argv[1] | ... | argv[n] |
 *
 *  becomes:
 *
 *    | argv[0] | runner_args[0] | ... | runner_args[n] | argv[m] |
 */
static int insert_runner_args(char **argv[])
{
	char *tmp2;
	char *tmp;
	int i;

	if (runner_args[0] == '\0')
		return 0;

	for (i = 0; (*argv)[i] != NULL; i++)
		;
	i++; /* Don't miss the trailing NULL terminator. */

	/* Don't kill *argv if the reallocation failed, all entries
	 * will be freed in translate_execve(). */
	tmp = realloc(*argv, (nb_runner_args + i) * sizeof(char *));
	if (tmp == NULL)
		return -ENOMEM;
	*argv = (void *)tmp;

	/* Move the old entries to let space at the beginning for the
	 * new ones. */
	memmove(*argv + 1 + nb_runner_args, *argv + 1, (i - 1) * sizeof(char *));

	/* Each new entries will be allocated into the heap since we
	 * don't rely on the liveness of the input parameters. */
	tmp = tmp2 = runner_args;
	for (i = 1; i <= nb_runner_args; i++) {
		assert(tmp != NULL);
		ptrdiff_t size;

		tmp = index(tmp, ',');
		if (tmp == NULL)
			size = strlen(tmp2) + 1;
		else {
			tmp++;
			size = tmp - tmp2;
		}

		(*argv)[i] = calloc(size, size * sizeof(char));
		if ((*argv)[i] == NULL)
			return -ENOMEM; /* XXX: *argv is inconsistent! */

		strncpy((*argv)[i], tmp2, size - 1);
		(*argv)[i][size] = '\0';

		tmp2 = tmp;
	}
	assert(tmp == NULL);

	return 0;
}

/**
 * Copy the *@argv[] of the current execve(2) from the memory space of
 * the @child process.  This function returns -errno if an error
 * occured, otherwise 0.
 */
static int get_argv(struct child_info *child, char **argv[])
{
	word_t child_argv;
	word_t argp;
	int nb_argv;
	int status;
	int i;

#ifdef ARCH_X86_64
#    define sizeof_word(child) ((child)->uregs == uregs \
				? sizeof(word_t)	\
				: sizeof(word_t) / 2)
#else
#    define sizeof_word(child) (sizeof(word_t))
#endif

	child_argv = peek_ureg(child, SYSARG_2);
	if (errno != 0)
		return -errno;

	/* Compute the number of entries in argv[]. */
	for (i = 0; ; i++) {
		argp = (word_t) ptrace(PTRACE_PEEKDATA, child->pid,
				       child_argv + i * sizeof_word(child), NULL);
		if (errno != 0)
			return -EFAULT;

#ifdef ARCH_X86_64
		/* Use only the 32 LSB when running 32-bit processes. */
		if (child->uregs == uregs2)
			argp &= 0xFFFFFFFF;
#endif
		/* End of argv[]. */
		if (argp == 0)
			break;
	}
	nb_argv = i;

	*argv = calloc(nb_argv + 1, sizeof(char *));
	if (*argv == NULL)
		return -ENOMEM;

	(*argv)[nb_argv] = NULL;
	
	/* Slurp arguments until the end of argv[]. */
	for (i = 0; i < nb_argv; i++) {
		char arg[ARG_MAX];

		argp = (word_t) ptrace(PTRACE_PEEKDATA, child->pid,
				       child_argv + i * sizeof_word(child), NULL);
		if (errno != 0)
			return -EFAULT;

#ifdef ARCH_X86_64
		/* Use only the 32 LSB when running 32-bit processes. */
		if (child->uregs == uregs2)
			argp &= 0xFFFFFFFF;
#endif
		assert(argp != 0);

		status = get_child_string(child, arg, argp, ARG_MAX);
		if (status < 0)
			return status;
		if (status >= ARG_MAX)
			return -ENAMETOOLONG;

		(*argv)[i] = calloc(status, sizeof(char));
		if ((*argv)[i] == NULL)
			return -ENOMEM;

		strcpy((*argv)[i], arg);
	}

	return 0;
}

/**
 * Copy the @argv[] to the memory space of the @child process.
 * This function returns -errno if an error occured, otherwise 0.
 *
 * Technically, we use the memory below the stack pointer to store the
 * new arguments and the new array of pointers to these arguments:
 *
 *                                          <- stack pointer
 *                                                          \
 *       argv[]           argv1              argv0           \
 *     /                       \                  \           \
 *    | argv[0] | argv[1] | ... | "/bin/script.sh" | "/bin/sh" |
 */
static int set_argv(struct child_info *child, char *argv[])
{
	word_t *child_args;

	word_t previous_sp;
	word_t child_argv;
	word_t argp;

	int nb_argv;
	size_t size;
	long status;
	int i;

	/* Compute the number of entries. */
	for (i = 0; argv[i] != NULL; i++)
		VERBOSE(4, "set argv[%d] = %s", i, argv[i]);

	nb_argv = i + 1;
	child_args = calloc(nb_argv, sizeof(word_t));
	if (child_args == NULL)
		return -ENOMEM;

	/* Copy the new arguments in the child's stack. */
	previous_sp = peek_ureg(child, STACK_POINTER);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	argp = previous_sp;
	for (i = 0; argv[i] != NULL; i++) {
		size = strlen(argv[i]) + 1;
		argp -= size;

		status = copy_to_child(child, argp, argv[i], size);
		if (status < 0)
			goto end;

		child_args[i] = argp;
	}

	child_args[i] = 0;
	child_argv = argp;

	/* Copy the pointers to the new arguments backward in the stack. */
	for (i = nb_argv - 1; i >= 0; i--) {
		child_argv -= sizeof_word(child);

#ifdef ARCH_X86_64
		/* Don't overwrite the "extra" 32-bit part. */
		if (child->uregs == uregs2) {
			argp = (word_t) ptrace(PTRACE_PEEKDATA, child->pid, child_argv, NULL);
			if (errno != 0)
				return -EFAULT;

			assert(child_args[i] >> 32 == 0);
			child_args[i] |= (argp & 0xFFFFFFFF00000000ULL);
		}
#endif

		status = ptrace(PTRACE_POKEDATA, child->pid, child_argv, child_args[i]);
		if (status <0) {
			status = -EFAULT;
			goto end;
		}
	}

	/* Update the pointer to the new argv[]. */
	status = poke_ureg(child, SYSARG_2, child_argv);
	if (status < 0)
		goto end;

	/* Update the stack pointer to ensure [internal] coherency. It prevents
	 * memory corruption if functions like set_sysarg_path() are called later. */
	status = poke_ureg(child, STACK_POINTER, child_argv);
	if (status < 0)
		goto end;

end:
	free(child_args);

	if (status < 0)
		return status;

	return previous_sp - child_argv;
}

static int expand_interp(struct child_info *child, char filename[PATH_MAX], char **argv[], expand_interp_t callback, int skip_interp_expansion)
{
	char path[PATH_MAX];
	struct stat statl;
	int status;

	/*
	 * Don't try to launch the program if it:
	 *     - doesn't exist; or
	 *     - isn't executable; or
	 *     - isn't a regular file.
	 */

	status = translate_path(child, path, AT_FDCWD, filename, REGULAR);
	if (status < 0)
		return -1;

	status = access(path, F_OK);
	if (status < 0)
		return -ENOENT;

	status = access(path, X_OK);
	if (status < 0)
		return -EACCES;

	status = lstat(path, &statl);
	if (status < 0 || !S_ISREG(statl.st_mode))
		return -EPERM;

	if (skip_interp_expansion) {
		strcpy(filename, path);
		return 0;
	}

	/* Expand the interpreter. */
	status = callback(child, path, filename, argv);
	if (status <= 0)
		return status;

	/* Sanity check: an interpreter doesn't request an[other]
	 * interpreter on Linux.  In the case of ELF this check
	 * ensures the interpreter is in the ELF format. */
	status = callback(child, filename, NULL, argv);
	if (status < 0)
		return status;

	return 1;
}

/**
 * Translate the arguments of the execve() syscall made by the @child
 * process. This syscall needs a very special treatment for script
 * files because according to "man 2 execve":
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
 * We can't just translate the first parameter because the kernel
 * will actually run the interpreter "/bin/sh" with the translated
 * path to the script file "/tmp/new_root/bin/script.sh" as its first
 * argument. Technically, we want the opposite behaviour, that is, we
 * want to run the translated path to the interpreter
 * "/tmp/new_root/bin/sh" with the de-translated path to the script
 * "/bin/script.sh" as its first parameter (will be translated later):
 *
 *     execve("/tmp/new_root/bin/sh", argv = [ "/bin/sh", "/bin/script.sh", "arg1", arg2", ... ], envp);
 */
int translate_execve(struct child_info *child)
{
	char filename[PATH_MAX];
	char **argv = NULL;
	char *argv0 = NULL;

	int nb_interp = 0;
	int size = 0;
	int status;
	int i;

	status = get_sysarg_path(child, filename, SYSARG_1);
	if (status < 0)
		return status;

	status = get_argv(child, &argv);
	if (status < 0)
		goto end;

	/* Save the original argv[0] so QEMU will pass it to the program. */
	if (runner_is_qemu && runner[0] != '\0')
		argv0 = strdup(argv[0]);

	status = expand_interp(child, filename, &argv, expand_script_interp, 0);
	if (status < 0)
		goto end;
	nb_interp += status;

	/* I prefer the binfmt_misc approach instead of invoking
	 * the runner/loader unconditionally. */
	if (runner[0] != '\0') {
		nb_interp++;

		if (argv0 != NULL)
			status = substitute_argv0(&argv, 4, runner, "-0", argv0, filename);
		else
			status = substitute_argv0(&argv, 2, runner, filename);
		if (status < 0)
			goto end;

		status = insert_runner_args(&argv);
		if (status < 0)
			goto end;

		/* Delayed the translation of the newly instantiated
		 * runner until it accesses the program to execute,
		 * that way the runner can first access its own files
		 * outside of the rootfs. */
		child->trigger = strdup(filename);
		if (child->trigger == NULL)
			return -ENOMEM;

		/* Launch the runner actually. */
		strcpy(filename, runner);
	}
	else {
		status = expand_interp(child, filename, &argv, expand_elf_interp, skip_elf_interp);
		if (status < 0)
			goto end;
		nb_interp += status;
	}

	/* Rebuild argv[] only if something has changed. */
	if (argv != NULL && nb_interp != 0) {
		size = set_argv(child, argv);
		if (size < 0) {
			status = size;
			goto end;
		}
	}

	VERBOSE(4, "execve: %s", filename);

	status = set_sysarg_path(child, filename, SYSARG_1);
	if (status < 0)
		goto end;

end:
	if (argv != NULL) {
		for (i = 0; argv[i] != NULL; i++)
			free(argv[i]);
		free(argv);
	}

	if (argv0 != NULL)
		free(argv0);

	if (status < 0)
		return status;

	return size + status;
}
