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
#include <sys/queue.h> /* SLIST_*, */
#include <talloc.h>    /* talloc_*, */

#include "cli.h"
#include "path/binding.h"
#include "notice.h"
#include "path/path.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "tracee/tracee.h"
#include "execve/ldso.h"
#include "build.h"

static void handle_option_r(Tracee *tracee, char *value)
{
	tracee->root = value;
}

typedef struct binding {
	const char *host;
	const char *guest;
	bool must_exist;

	SLIST_ENTRY(binding) link;
} Binding;

static SLIST_HEAD(bindings, binding) bindings;

static void new_binding2(const char *host, const char *guest, bool must_exist)
{
	Binding *binding;

	binding = talloc_zero(NULL, Binding);
	if (binding == NULL) {
		notice(WARNING, INTERNAL, "talloc_zero() failed");
		return;
	}

	binding->host  = host;
	binding->guest = guest;
	binding->must_exist = must_exist;

	SLIST_INSERT_HEAD(&bindings, binding, link);
}

static void new_binding(char *value, bool must_exist)
{
	char *ptr = strchr(value, ':');
	if (ptr != NULL) {
		*ptr = '\0';
		ptr++;
	}

	/* Expand environment variables like $HOME.  */
	if (value[0] == '$' && getenv(&value[1]))
		value = getenv(&value[1]);

	new_binding2(value, ptr, must_exist);
}

static void handle_option_b(Tracee *tracee, char *value)
{
	new_binding(value, true);
}

/**
 * Put in @result the absolute path of the given @command.
 *
 * This function always returns something consistent or die trying.
 */
static void which(char *const command, char result[PATH_MAX])
{
	char *const argv[3] = { "which", command, NULL };
	char which_output[PATH_MAX];
	int pipe_fd[2];
	int status;
	pid_t pid;

	status = pipe(pipe_fd);
	if (status < 0)
		notice(ERROR, SYSTEM, "pipe()");

	pid = fork();
	switch (pid) {
	case -1:
		notice(ERROR, SYSTEM, "fork()");

	case 0: /* child */
		close(pipe_fd[0]); /* "read" end */

		/* Replace the standard output with the "write" end of
		 * the pipe.  */
		status = dup2(pipe_fd[1], STDOUT_FILENO);
		if (status < 0)
			notice(ERROR, SYSTEM, "dup2()");

		execvp(argv[0], argv);
		notice(ERROR, SYSTEM, "can't execute `%s %s`", argv[0], command);

	default: /* parent */
		close(pipe_fd[1]); /* "write" end */

		status = read(pipe_fd[0], which_output, PATH_MAX - 1);
		if (status < 0)
			notice(ERROR, SYSTEM, "read()");
		if (status == 0)
			notice(ERROR, USER, "%s: command not found", command);
		assert(status < PATH_MAX);
		which_output[status - 1] = '\0'; /* overwrite "\n" */

		close(pipe_fd[0]); /* "read" end */

		pid = wait(&status);
		if (pid < 0)
			notice(ERROR, SYSTEM, "wait()");

		if (status != 0)
			notice(ERROR, USER, "`%s %s` returned an error", argv[0], command);
		break;
	}

	if (realpath(which_output, result) == NULL)
		notice(ERROR, SYSTEM, "realpath(\"%s\")", which_output);
}

static void handle_option_q(Tracee *tracee, char *value)
{
	char path[PATH_MAX];
	size_t nb_args;
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
	if (!tracee->qemu)
		notice(ERROR, INTERNAL, "talloc_zero_array() failed");

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

	which(tracee->qemu[0], path);
	tracee->qemu[0] = talloc_strdup(tracee->qemu, path);

	new_binding2("/", HOST_ROOTFS, true);
	new_binding2("/dev/null", "/etc/ld.so.preload", false);
}

static void handle_option_w(Tracee *tracee, char *value)
{
	tracee->cwd = value;
}

static void handle_option_k(Tracee *tracee, char *value)
{
	tracee->kernel_release = value;
}

static void handle_option_0(Tracee *tracee, char *value)
{
	tracee->fake_id0 = true;
}

static void handle_option_v(Tracee *tracee, char *value)
{
	char *end_ptr = NULL;

	errno = 0;
	verbose_level = strtol(value, &end_ptr, 10);
	if (errno != 0 || end_ptr == value)
		notice(ERROR, USER, "option `-v` expects an integer value.");
}

static void handle_option_V(Tracee *tracee, char *value)
{
	printf("PRoot %s: %s.\n", version, subtitle);
	printf("%s\n", colophon);
	exit(EXIT_SUCCESS);
}

static void print_usage(bool);
static void handle_option_h(Tracee *tracee, char *value)
{
	print_usage(true);
	exit(EXIT_SUCCESS);
}

static void handle_option_B(Tracee *tracee, char *value)
{
	int i;
	for (i = 0; recommended_bindings[i] != NULL; i++)
		new_binding(recommended_bindings[i], false);
}

static void handle_option_Q(Tracee *tracee, char *value)
{
	handle_option_q(tracee, value);
	handle_option_B(tracee, NULL);
}

#define NB_OPTIONS (sizeof(options) / sizeof(Option))

/**
 * Print a (@detailed) usage of PRoot.
 */
static void print_usage(bool detailed)
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

	if (detailed)
		printf("%s\n", colophon);
}

static char *default_command[] = { "/bin/sh", NULL };

static void print_execve_help(const char *argv0)
{
	notice(WARNING, SYSTEM, "execv(\"%s\")", argv0);
	notice(INFO, USER, "possible causes:\n"
"  * <program> is a script but its interpreter (eg. /bin/sh) was not found;\n"
"  * <program> is an ELF but its interpreter (eg. ld-linux.so) was not found;\n"
"  * <program> is a foreign binary but no <qemu> was specified;\n"
"  * <qemu> does not work correctly (if specified).");
}

static void error_separator(Argument *argument)
{
	if (argument->separator == '\0')
		notice(ERROR, USER,
			"option '%s' expects no value.",
			argument->name);
	else
		notice(ERROR, USER,
			"option '%s' and its value must be separated by '%c'.",
			argument->name,
			argument->separator);
}

static void print_argv(const char *prompt, char **argv)
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

static void print_config(const Tracee *tracee)
{
	notice(INFO, USER, "guest rootfs = %s", tracee->root);

	if (tracee->qemu)
		notice(INFO, USER, "host rootfs = %s", HOST_ROOTFS);

	if (tracee->glue)
		notice(INFO, USER, "glue rootfs = %s", tracee->glue);

	print_argv("command", tracee->cmdline);
	print_argv("qemu", tracee->qemu);

	if (tracee->cwd)
		notice(INFO, USER, "initial cwd = %s", tracee->cwd);

	if (tracee->kernel_release)
		notice(INFO, USER, "kernel release = %s", tracee->kernel_release);

	if (tracee->fake_id0)
		notice(INFO, USER, "fake root id = true");

	notice(INFO, USER, "verbose level = %d", verbose_level);

	print_bindings();
}

/**
 * Configure @tracee according to the command-line arguments stored in
 * @argv[].  This function succeed or die trying.
 */
static void parse_cli(Tracee *tracee, int argc, char *argv[])
{
	option_handler_t handler = NULL;
	Binding *binding;
	char tmp[PATH_MAX];
	int i, j, k;
	int status;

	if (argc == 1) {
		print_usage(false);
		exit(EXIT_FAILURE);
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		/* The current argument is the value of a short option.  */
		if (handler != NULL) {
			handler(tracee, arg);
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
				    && arg[length] != argument->separator)
					error_separator(argument);

				/* No option value.  */
				if (!argument->value) {
					option->handler(tracee, arg);
					goto known_option;
				}

				/* Value coalesced with to its option.  */
				if (argument->separator == arg[length]) {
					assert(strlen(arg) >= length);
					option->handler(tracee, &arg[length + 1]);
					goto known_option;
				}

				/* Avoid ambiguities.  */
				if (argument->separator != ' ')
					error_separator(argument);

				/* Short option with a separated value.  */
				handler = option->handler;
				goto known_option;
			}
		}

		notice(ERROR, USER, "unknown option '%s'.", arg);

	known_option:
		if (handler != NULL && i == argc - 1)
			notice(ERROR, USER,
				"missing value for option '%s'.", arg);
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

	if (realpath(tracee->root, tmp) == NULL)
		notice(ERROR, SYSTEM, "realpath(\"%s\")", tracee->root);
	tracee->root = talloc_strdup(tracee, tmp);

	/* The binding to "/" has to be initialized before other
	 * bindings since this former is required to sanitize these
	 * latters (c.f. bind_path() for details).  */
	status = bind_path(tracee, tracee->root, "/", true);
	if (status < 0)
		notice(ERROR, USER, "fatal");

	binding = SLIST_FIRST(&bindings);
	while (binding != NULL) {
		Binding *next = SLIST_NEXT(binding, link);
		bind_path(tracee, binding->host, binding->guest, binding->must_exist);
		TALLOC_FREE(binding);
		binding = next;
	}

	if (i < argc)
		tracee->cmdline = &argv[i];
	else
		tracee->cmdline = default_command;

	if (verbose_level > 0)
		print_config(tracee);
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

	/* Pre-configure the first tracee.  */
	parse_cli(tracee, argc, argv);

	/* Start the first tracee.  */
	status = launch_process(tracee);
	if (!status) {
		print_execve_help(tracee->cmdline[0]);
		exit(EXIT_FAILURE);
	}

	/* Start tracing the first tracee and all its children.  */
	exit(event_loop());
}
