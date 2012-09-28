/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
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

#define _GNU_SOURCE    /* get_current_dir_name(3), */
#include <stdio.h>     /* printf(3), */
#include <string.h>    /* string(3), */
#include <stdlib.h>    /* exit(3), strtol(3), */
#include <stdbool.h>   /* bool, true, false, */
#include <assert.h>    /* assert(3), */
#include <unistd.h>    /* acess(2), pipe(2), dup2(2), */
#include <sys/wait.h>  /* wait(2), */
#include <sys/types.h> /* stat(2), */
#include <sys/stat.h>  /* stat(2), */
#include <unistd.h>    /* stat(2), */
#include <errno.h>     /* errno(3), */
#include <linux/limits.h> /* PATH_MAX, ARG_MAX, */
#include <sys/queue.h> /* LIST_*, */
#include <talloc.h>    /* talloc_*, */

#include "cli.h"
#include "notice.h"
#include "path/path.h"
#include "path/binding.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "extension/extension.h"
#include "build.h"

static int handle_option_r(Tracee *tracee, char *value)
{
	tracee->root = value;
	return 0;
}

static int handle_option_b(Tracee *tracee, char *value)
{
	char *ptr = strchr(value, ':');
	if (ptr != NULL) {
		*ptr = '\0';
		ptr++;
	}

	new_binding(tracee, value, ptr, true);
	return 0;
}

/**
 * Put in @result the absolute path of the given @command.
 *
 * This function always returns -1 on error, toherwsie 0.
 */
static int which(char *const command, char result[PATH_MAX])
{
	char *const argv[3] = { "which", command, NULL };
	char which_output[PATH_MAX];
	int pipe_fd[2];
	int status;
	pid_t pid;

	status = pipe(pipe_fd);
	if (status < 0) {
		notice(ERROR, SYSTEM, "pipe()");
		return -1;
	}

	pid = fork();
	switch (pid) {
	case -1:
		notice(ERROR, SYSTEM, "fork()");
		return -1;

	case 0: /* child */
		close(pipe_fd[0]); /* "read" end */

		/* Replace the standard output with the "write" end of
		 * the pipe.  */
		status = dup2(pipe_fd[1], STDOUT_FILENO);
		if (status < 0) {
			notice(ERROR, SYSTEM, "dup2()");
			return -1;
		}

		execvp(argv[0], argv);

		notice(ERROR, SYSTEM, "can't execute `%s %s`", argv[0], command);
		return -1;

	default: /* parent */
		close(pipe_fd[1]); /* "write" end */

		status = read(pipe_fd[0], which_output, PATH_MAX - 1);
		if (status < 0) {
			notice(ERROR, SYSTEM, "read()");
			return -1;
		}
		if (status == 0) {
			notice(ERROR, USER, "%s: command not found", command);
			return -1;
		}
		assert(status < PATH_MAX);
		which_output[status - 1] = '\0'; /* overwrite "\n" */

		close(pipe_fd[0]); /* "read" end */

		pid = wait(&status);
		if (pid < 0) {
			notice(ERROR, SYSTEM, "wait()");
			return -1;
		}
		if (status != 0) {
			notice(ERROR, USER, "`%s %s` returned an error", argv[0], command);
			return -1;
		}
		break;
	}

	if (realpath(which_output, result) == NULL) {
		notice(ERROR, SYSTEM, "realpath(\"%s\")", which_output);
		return -1;
	}

	return 0;
}

static int handle_option_q(Tracee *tracee, char *value)
{
	char path[PATH_MAX];
	size_t nb_args;
	int status;
	char *ptr;
	int i;

	nb_args = 0;
	ptr = value;
	while (1) {
		nb_args++;

		/* Keep consecutive non-space characters.  */
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;

		/* End-of-string ?  */
		if (*ptr == '\0')
			break;

		/* Skip consecutive space separators.  */
		while (*ptr == ' ' && *ptr != '\0')
			ptr++;

		/* End-of-string ?  */
		if (*ptr == '\0')
			break;
	}

	tracee->qemu = talloc_zero_array(tracee, char *, nb_args + 1);
	if (!tracee->qemu) {
		notice(ERROR, INTERNAL, "talloc_zero_array() failed");
		return -1;
	}
	talloc_set_name_const(tracee->qemu, "@qemu");

	i = 0;
	ptr = value;
	while (1) {
		tracee->qemu[i] = ptr;
		i++;

		/* Keep consecutive non-space characters.  */
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;

		/* End-of-string ?  */
		if (*ptr == '\0')
			break;

		/* Remove consecutive space separators.  */
		while (*ptr == ' ' && *ptr != '\0')
			*ptr++ = '\0';

		/* End-of-string ?  */
		if (*ptr == '\0')
			break;
	}
	assert(i == nb_args);

	status = which(tracee->qemu[0], path);
	if (status < 0)
		return -1;
	tracee->qemu[0] = talloc_strdup(tracee->qemu, path);

	new_binding(tracee, "/", HOST_ROOTFS, true);
	new_binding(tracee, "/dev/null", "/etc/ld.so.preload", false);

	return 0;
}

static int handle_option_w(Tracee *tracee, char *value)
{
	tracee->cwd = talloc_strdup(tracee, value);
	talloc_set_name_const(tracee->cwd, "$cwd");
	return 0;
}

static int handle_option_k(Tracee *tracee, char *value)
{
	int status;

	status = initialize_extension(tracee, kompat_callback, value);
	if (status < 0)
		notice(WARNING, INTERNAL, "option \"-k %s\" discarded", value);

	return 0;
}

static int handle_option_0(Tracee *tracee, char *value)
{
	(void) initialize_extension(tracee, fake_id0_callback, value);
	return 0;
}

static int handle_option_v(Tracee *tracee, char *value)
{
	char *end_ptr = NULL;

	errno = 0;
	verbose_level = strtol(value, &end_ptr, 10);
	if (errno != 0 || end_ptr == value) {
		notice(ERROR, USER, "option `-v` expects an integer value.");
		return -1;
	}

	return 0;
}

static bool exit_failure = true;

static int handle_option_V(Tracee *tracee, char *value)
{
	printf("PRoot %s: %s.\n", version, subtitle);
	printf("%s\n", colophon);
	exit_failure = false;
	return -1;
}

static void print_usage(Tracee *, bool);
static int handle_option_h(Tracee *tracee, char *value)
{
	print_usage(tracee, true);
	exit_failure = false;
	return -1;
}

static int handle_option_B(Tracee *tracee, char *value)
{
	int i;
	for (i = 0; recommended_bindings[i] != NULL; i++)
		new_binding(tracee, recommended_bindings[i], NULL, false);
	return 0;
}

static int handle_option_Q(Tracee *tracee, char *value)
{
	int status;

	status = handle_option_q(tracee, value);
	if (status < 0)
		return status;

	status = handle_option_B(tracee, NULL);
	if (status < 0)
		return status;

	return 0;
}

#define NB_OPTIONS (sizeof(options) / sizeof(Option))

/**
 * Print a (@detailed) usage of PRoot.
 */
static void print_usage(Tracee *tracee, bool detailed)
{
	const char *current_class = "none";
	int i, j;

#define DETAIL(a) if (detailed) a

	DETAIL(printf("PRoot %s: %s.\n\n", version, subtitle));
	printf("Usage:\n  %s\n", synopsis);
	DETAIL(printf("\n"));

	for (i = 0; i < NB_OPTIONS; i++) {
		for (j = 0; ; j++) {
			Argument *argument = &(options[i].arguments[j]);

			if (!argument->name || (!detailed && j != 0)) {
				DETAIL(printf("\n"));
				printf("\t%s\n", options[i].description);
				if (detailed) {
					if (options[i].detail[0] != '\0')
						printf("\n%s\n\n", options[i].detail);
					else
						printf("\n");
				}
				break;
			}

			if (strcmp(options[i].class, current_class) != 0) {
				current_class = options[i].class;
				printf("%s:\n", current_class);
			}

			if (j == 0)
				printf("  %s", argument->name);
			else
				printf(", %s", argument->name);

			if (argument->separator != '\0')
				printf("%c*%s*", argument->separator, argument->value);
			else if (!detailed)
				printf("\t");
		}
	}

	notify_extensions(tracee, PRINT_USAGE, detailed, 0);

	if (detailed)
		printf("%s\n", colophon);
}

static char *default_command[] = { "/bin/sh", NULL };

static void print_execve_help(const Tracee *tracee, const char *argv0)
{
	notice(WARNING, SYSTEM, "execv(\"%s\")", argv0);
	notice(INFO, USER, "possible causes:\n"
"  * <program> is a script but its interpreter (eg. /bin/sh) was not found;\n"
"  * <program> is an ELF but its interpreter (eg. ld-linux.so) was not found;\n"
"  * <program> is a foreign binary but no <qemu> was specified;\n"
"  * <qemu> does not work correctly (if specified).");
}

static void print_error_separator(const Tracee *tracee, Argument *argument)
{
	if (argument->separator == '\0')
		notice(ERROR, USER, "option '%s' expects no value.", argument->name);
	else
		notice(ERROR, USER,
			"option '%s' and its value must be separated by '%c'.",
			argument->name, argument->separator);
}

static void print_argv(const Tracee *tracee, const char *prompt, char **argv)
{
	int i;
	char string[ARG_MAX] = "";

	if (!argv)
		return;

	void append(const char *post) {
		ssize_t length = sizeof(string) - (strlen(string) + strlen(post));
		if (length <= 0)
			return;
		strncat(string, post, length);
	}

	append(prompt);
	append(" =");
	for (i = 0; argv[i] != NULL; i++) {
		append(" ");
		append(argv[i]);
	}
	string[sizeof(string) - 1] = '\0';

	notice(INFO, USER, "%s", string);
}

static void print_config(Tracee *tracee)
{
	notice(INFO, USER, "guest rootfs = %s", tracee->root);

	if (tracee->qemu)
		notice(INFO, USER, "host rootfs = %s", HOST_ROOTFS);

	if (tracee->glue)
		notice(INFO, USER, "glue rootfs = %s", tracee->glue);

	print_argv(tracee, "command", tracee->cmdline);
	print_argv(tracee, "qemu", tracee->qemu);

	if (tracee->cwd)
		notice(INFO, USER, "initial cwd = %s", tracee->cwd);

	notice(INFO, USER, "verbose level = %d", verbose_level);

	notify_extensions(tracee, PRINT_CONFIG, 0, 0);
}

/**
 * Configure @tracee according to the command-line arguments stored in
 * @argv[].  This function succeed or die trying.
 */
static int parse_cli(Tracee *tracee, int argc, char *argv[])
{
	option_handler_t handler = NULL;
	Binding *binding;
	int i, j, k;
	int status;

	if (argc == 1) {
		print_usage(tracee, false);
		return -1;
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		/* The current argument is the value of a short option.  */
		if (handler != NULL) {
			status = handler(tracee, arg);
			if (status < 0)
				return status;
			handler = NULL;
			continue;
		}

		if (arg[0] != '-')
			break; /* End of PRoot options. */

		for (j = 0; j < NB_OPTIONS; j++) {
			Option *option = &options[j];

			/* A given option has several aliases.  */
			for (k = 0; ; k++) {
				Argument *argument;
				size_t length;

				argument = &option->arguments[k];

				/* End of aliases for this option.  */
				if (!argument->name)
					break;

				length = strlen(argument->name);
				if (strncmp(arg, argument->name, length) != 0)
					continue;

				/* Avoid ambiguities.  */
				if (strlen(arg) > length
				    && arg[length] != argument->separator) {
					print_error_separator(tracee, argument);
					return -1;
				}

				/* No option value.  */
				if (!argument->value) {
					status = option->handler(tracee, arg);
					if (status < 0)
						return status;
					goto known_option;
				}

				/* Value coalesced with to its option.  */
				if (argument->separator == arg[length]) {
					assert(strlen(arg) >= length);
					status = option->handler(tracee, &arg[length + 1]);
					if (status < 0)
						return status;
					goto known_option;
				}

				/* Avoid ambiguities.  */
				if (argument->separator != ' ') {
					print_error_separator(tracee, argument);
					return -1;
				}

				/* Short option with a separated value.  */
				handler = option->handler;
				goto known_option;
			}
		}

		notice(ERROR, USER, "unknown option '%s'.", arg);
		return -1;

	known_option:
		if (handler != NULL && i == argc - 1) {
			notice(ERROR, USER, "missing value for option '%s'.", arg);
			return -1;
		}
	}

	/* When no guest rootfs were specified: if the first bare
	 * option is a directory, then the old command-line interface
	 * (similar to the chroot one) is expected.  Otherwise this is
	 * the new command-line interface where the default guest
	 * rootfs is "/".
	 */
	if (tracee->root == NULL) {
		struct stat buf;

		status = stat(argv[i], &buf);
		if (status == 0 && S_ISDIR(buf.st_mode))
			tracee->root = argv[i++];
		else
			tracee->root = "/";
	}

	/* Note: ``chroot $PATH`` is almost equivalent to ``mount
	 * --bind $PATH /``.  PRoot just ensures it is inserted before
	 * other bindings since this former is required to
	 * canonicalize these latters.  */
	binding = new_binding(tracee, tracee->root, "/", true);
	if (binding == NULL)
		return -1;

	/* tracee->root is mainly used to avoid costly lookups in
	 * tracee->bindings.guest.  */
	tracee->root = talloc_strdup(tracee, binding->host.path);
	talloc_set_name_const(tracee->root, "$root");

	/* Bindings specify by the user (tracee->bindings.user) can
	 * now be canonicalized, as expected by get_bindings().  */
	status = initialize_bindings(tracee);
	if (status < 0)
		return -1;

	if (i < argc)
		tracee->cmdline = &argv[i];
	else
		tracee->cmdline = default_command;

	if (verbose_level > 0)
		print_config(tracee);

	return 0;
}

int main(int argc, char *argv[])
{
	Tracee *tracee;
	int status;

	/* Configure the memory allocator.  */
	talloc_enable_leak_report();
	talloc_set_log_stderr();

	/* Pre-create the first tracee (pid == 0).  */
	tracee = get_tracee(0, true);
	assert(tracee != NULL);

	/* Pre-configure the first tracee.  */
	status = parse_cli(tracee, argc, argv);
	if (status < 0)
		goto error;

	/* Start the first tracee.  */
	status = launch_process(tracee);
	if (status < 0) {
		print_execve_help(tracee, tracee->cmdline[0]);
		goto error;
	}

	/* Start tracing the first tracee and all its children.  */
	exit(event_loop());

error:
	TALLOC_FREE(tracee);

	if (exit_failure) {
		fprintf(stderr, "proot error: see `proot --help` or `man proot`.\n");
		exit(EXIT_FAILURE);
	}
	else
		exit(EXIT_SUCCESS);
}
