/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
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

#include <stdlib.h>     /* *alloc(3), free(3), */
#include <string.h>     /* string(3), */
#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <errno.h>      /* E*, */
#include <stdarg.h>     /* va_*(3), */
#include <assert.h>     /* assert(3), */

#include "execve/args.h"
#include "tracee/info.h"
#include "tracee/ureg.h"
#include "tracee/mem.h"
#include "notice.h"
#include "syscall/syscall.h"
#include "config.h"

static char runner_args[ARG_MAX] = { '\0', };
static int nb_runner_args;

void init_runner_args(const char *runner)
{
	char *tmp;

	/* Split apart the name of the runner and its options, if any. */
	tmp = index(runner, ',');
	if (tmp != NULL) {
		*tmp = '\0';
		tmp++;

		if (strlen(tmp) >= ARG_MAX)
			notice(ERROR, USER, "the option \"-q %s\" is too long", runner);

		strcpy(runner_args, tmp);
		for (nb_runner_args = 1; (tmp = index(tmp, ',')) != NULL; nb_runner_args++)
			tmp++;
	}
}

/**
 * Push into the environment pointed to by *@envp the entry @env
 * (format is "variable=new_value"). If the variable is already
 * defined in the environment then the old entry if copied into
 * @old_env (format is "variable=old_value").
 */
int push_env(char **envp[], const char *env, char old_env[ARG_MAX])
{
	size_t new_size;
	size_t length;
	char *tmp;
	int i;

	old_env[0] = '\0';

	length = strcspn(env, "=");
	if (length == strlen(env))
		return -EINVAL;

	/* Seach for the environment variable. */
	for (i = 0; (*envp)[i] != NULL; i++) {
		const char *value;

		if (   strncmp(env, (*envp)[i], length) != 0
		    || (*envp)[i][length] != '=')
			continue;

		if (strlen((*envp)[i]) < ARG_MAX)
			strcpy(old_env, (*envp)[i]);

		/* "value" includes the separator '='. */
		value = env + length;

		tmp = realloc((*envp)[i], (length + strlen(value) + 1) * sizeof(char));
		if (tmp == NULL)
			return  -ENOMEM;
		(*envp)[i] = tmp;

		strcpy((*envp)[i] + length, value);
		return 0;
	}

	/* Allocate a new entry + the terminating NULL pointer. */
	new_size = i + 2;

	tmp = realloc(*envp, new_size * sizeof(char *));
	if (tmp == NULL)
		return -ENOMEM;
	*envp = (char **)tmp;

	(*envp)[new_size - 2] = strdup(env);
	if ((*envp)[new_size - 2] == NULL)
		return -ENOMEM;

	(*envp)[new_size - 1] = NULL;

	return 0;
}

/**
 * Add @nb_new_args new C strings to the argument list *@args.  If
 * @replace_argv0 is true then the previous args[0] is freed and
 * replaced with the first new entry. This function returns -errno if
 * an error occured, otherwise 0.
 *
 *  Technically:
 *
 *    | old[0] | old[1] | ... | old[n] |
 *
 *  becomes as follow when !replace_argv0:
 *
 *    | old[0] | new[0] | ... | new[nb - 1] | old[1] | ... | old[n] |
 *
 *  otherwise as follow:
 *
 *    | new[0] | ... | new[nb - 1] | old[1] | ... | old[n] |
 */
int push_args(bool replace_argv0, char **args[], int nb_new_args, ...)
{
	char **dest_old_args;
	va_list va_args;
	void *tmp;

	int nb_moved_old_args;
	int nb_old_args;
	int i;

	assert(nb_new_args > 0);

	/* Count the number of old entries... */
	for (i = 0; (*args)[i] != NULL; i++)
		;
	nb_old_args = i + 1; /* ... including NULL terminator slot... */

	/* ... minus one if the args[0] slot is re-used. */
	if (replace_argv0)
		nb_old_args--;
	assert(nb_old_args > 0);

	/* Don't kill *args if the reallocation has failed, all
	 * entries will be freed in translate_execve(). */
	tmp = realloc(*args, (nb_new_args + nb_old_args) * sizeof(char *));
	if (tmp == NULL)
		return -ENOMEM;
	*args = tmp;

	/* Compute the new position and the number of moved old
	 * entries: they are shifted to the right.  */
	nb_moved_old_args = nb_old_args;
	dest_old_args = *args + nb_new_args;
	if (!replace_argv0) {
		dest_old_args++;
		nb_moved_old_args--;
	}
	else {
		free(*args[0]);
		*args[0] = NULL;
	}

	/* Move the old entries to let space at the beginning for the
	 * new ones, note that args[0] is not moved whether it is
	 * overwritten or not. */
	memmove(dest_old_args, *args + 1, nb_moved_old_args * sizeof(char *));

	/* Each new entry will be allocated into the heap since we
	 * don't rely on the liveness of the input parameters. */
	va_start(va_args, nb_new_args);
	for (i = 0; i < nb_new_args; i++) {
		int dest_index = i;
		char *new_arg = va_arg(va_args, char *);

		if (!replace_argv0)
			dest_index++;

		(*args)[dest_index] = strdup(new_arg);
		if ((*args)[dest_index] == NULL) {
			va_end(va_args);
			return -ENOMEM;
		}
	}
	va_end(va_args);

	return 0;
}

/**
 * Insert each entry of runner_args[] in between the first entry of
 * *@args and its sucessor.  This function returns -errno if an error
 * occured, otherwise 0.
 *
 *  Technically:
 *
 *    | args[0] | args[1] | ... | args[n] |
 *
 *  becomes:
 *
 *    | args[0] | runner_args[0] | ... | runner_args[n] | args[m] |
 */
int insert_runner_args(char **args[])
{
	char *tmp2;
	char *tmp;
	int i;

	if (runner_args[0] == '\0')
		return 0;

	for (i = 0; (*args)[i] != NULL; i++)
		;
	i++; /* Don't miss the trailing NULL terminator. */

	/* Don't kill *args if the reallocation failed, all entries
	 * will be freed in translate_execve(). */
	tmp = realloc(*args, (nb_runner_args + i) * sizeof(char *));
	if (tmp == NULL)
		return -ENOMEM;
	*args = (void *)tmp;

	/* Move the old entries to let space at the beginning for the
	 * new ones. */
	memmove(*args + 1 + nb_runner_args, *args + 1, (i - 1) * sizeof(char *));

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

		(*args)[i] = calloc(size, size * sizeof(char));
		if ((*args)[i] == NULL)
			return -ENOMEM;

		strncpy((*args)[i], tmp2, size - 1);
		(*args)[i][size] = '\0';

		tmp2 = tmp;
	}
	assert(tmp == NULL);

	return 0;
}

/**
 * Copy the *@args[] of the current execve(2) from the memory space of
 * the @tracee process.  This function returns -errno if an error
 * occured, otherwise 0.
 */
int get_args(struct tracee_info *tracee, char **args[], enum sysarg sysarg)
{
	word_t tracee_args;
	word_t argp;
	int nb_args;
	int status;
	int i;

#ifdef ARCH_X86_64
#    define sizeof_word(tracee) ((tracee)->uregs == uregs \
				? sizeof(word_t)	\
				: sizeof(word_t) / 2)
#else
#    define sizeof_word(tracee) (sizeof(word_t))
#endif

	tracee_args = peek_ureg(tracee, sysarg);
	if (errno != 0)
		return -errno;

	/* Compute the number of entries in args[]. */
	for (i = 0; ; i++) {
		argp = (word_t) ptrace(PTRACE_PEEKDATA, tracee->pid,
				       tracee_args + i * sizeof_word(tracee), NULL);
		if (errno != 0)
			return -EFAULT;

#ifdef ARCH_X86_64
		/* Use only the 32 LSB when running 32-bit processes. */
		if (tracee->uregs == uregs2)
			argp &= 0xFFFFFFFF;
#endif
		/* End of args[]. */
		if (argp == 0)
			break;
	}
	nb_args = i;

	*args = calloc(nb_args + 1, sizeof(char *));
	if (*args == NULL)
		return -ENOMEM;

	(*args)[nb_args] = NULL;

	/* Slurp arguments until the end of args[]. */
	for (i = 0; i < nb_args; i++) {
		char arg[ARG_MAX];

		argp = (word_t) ptrace(PTRACE_PEEKDATA, tracee->pid,
				       tracee_args + i * sizeof_word(tracee), NULL);
		if (errno != 0)
			return -EFAULT;

#ifdef ARCH_X86_64
		/* Use only the 32 LSB when running 32-bit processes. */
		if (tracee->uregs == uregs2)
			argp &= 0xFFFFFFFF;
#endif
		assert(argp != 0);

		status = get_tracee_string(tracee, arg, argp, ARG_MAX);
		if (status < 0)
			return status;
		if (status >= ARG_MAX)
			return -ENAMETOOLONG;

		(*args)[i] = strdup(arg);
		if ((*args)[i] == NULL)
			return -ENOMEM;
	}

	return 0;
}

/**
 * Copy the @args[] to the memory space of the @tracee process.
 * This function returns -errno if an error occured, otherwise 0.
 *
 * Technically, we use the memory below the stack pointer to store the
 * new arguments and the new array of pointers to these arguments:
 *
 *                                          <- stack pointer
 *                                                          \
 *       args[]           args1              args0           \
 *     /                       \                  \           \
 *    | args[0] | args[1] | ... | "/bin/script.sh" | "/bin/sh" |
 */
int set_args(struct tracee_info *tracee, char *args[], enum sysarg sysarg)
{
	word_t *tracee_args;

	word_t previous_sp;
	word_t tracee_argp;
	word_t argp;

	int nb_args;
	size_t size;
	long status;
	int i;

	/* Compute the number of entries. */
	for (i = 0; args[i] != NULL; i++)
		VERBOSE(4, "set args[%d] = %s", i, args[i]);

	nb_args = i + 1;
	tracee_args = calloc(nb_args, sizeof(word_t));
	if (tracee_args == NULL)
		return -ENOMEM;

	/* Copy the new arguments in the tracee's stack. */
	previous_sp = peek_ureg(tracee, STACK_POINTER);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	argp = previous_sp;
	for (i = 0; args[i] != NULL; i++) {
		size = strlen(args[i]) + 1;
		argp -= size;

		status = copy_to_tracee(tracee, argp, args[i], size);
		if (status < 0)
			goto end;

		tracee_args[i] = argp;
	}

	tracee_args[i] = 0;
	tracee_argp = argp;

	/* Copy the pointers to the new arguments backward in the stack. */
	for (i = nb_args - 1; i >= 0; i--) {
		tracee_argp -= sizeof_word(tracee);

#ifdef ARCH_X86_64
		/* Don't overwrite the "extra" 32-bit part. */
		if (tracee->uregs == uregs2) {
			argp = (word_t) ptrace(PTRACE_PEEKDATA, tracee->pid, tracee_argp, NULL);
			if (errno != 0)
				return -EFAULT;

			assert(tracee_args[i] >> 32 == 0);
			tracee_args[i] |= (argp & 0xFFFFFFFF00000000ULL);
		}
#endif
		status = ptrace(PTRACE_POKEDATA, tracee->pid, tracee_argp, tracee_args[i]);
		if (status <0) {
			status = -EFAULT;
			goto end;
		}
	}

	/* Update the pointer to the new args[]. */
	status = poke_ureg(tracee, sysarg, tracee_argp);
	if (status < 0)
		goto end;

	/* Update the stack pointer to ensure [internal] coherency. It prevents
	 * memory corruption if functions like set_sysarg_path() are called later. */
	status = poke_ureg(tracee, STACK_POINTER, tracee_argp);
	if (status < 0)
		goto end;

end:
	free(tracee_args);
	tracee_args = NULL;

	if (status < 0)
		return status;

	return previous_sp - tracee_argp;
}

/** Free each element in @args[] */
void free_args(char *args[])
{
	int i;

	if (args == NULL)
		return;

	for (i = 0; args[i] != NULL; i++) {
		free(args[i]);
		args[i] = NULL;
	}

	free(args);
	args = NULL;
}
