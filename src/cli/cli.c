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

#define _GNU_SOURCE        /* basename(3) GNU style, */
#include <stdbool.h>       /* bool, true, false,  */
#include <linux/limits.h>  /* ARG_MAX, PATH_MAX, */
#include <string.h>        /* str*(3), basename(3),  */
#include <talloc.h>        /* talloc*,  */
#include <stdlib.h>        /* exit(3), EXIT_*,  */
#include <assert.h>        /* assert(3),  */
#include <sys/types.h>     /* getpid(2),  */
#include <unistd.h>        /* getpid(2),  */
#include <errno.h>         /* errno(3), */
#include <stdlib.h>        /* strtol(3), */

#include "cli/cli.h"
#include "cli/notice.h"
#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "path/binding.h"
#include "path/canon.h"
#include "path/path.h"

/**
 * Print a (@detailed) usage of PRoot.
 */
void print_usage(Tracee *tracee, const Cli *cli, bool detailed)
{
	const char *current_class = "none";
	const Option *options;
	size_t i, j;

#define DETAIL(a) if (detailed) a

	DETAIL(printf("%s %s: %s.\n\n", cli->name, cli->version, cli->subtitle));
	printf("Usage:\n  %s\n", cli->synopsis);
	DETAIL(printf("\n"));

	options = cli->options;
	for (i = 0; options[i].class != NULL; i++) {
		for (j = 0; ; j++) {
			const Argument *argument = &(options[i].arguments[j]);

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
				printf("\n%s:\n", current_class);
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
		printf("%s\n", cli->colophon);
}

/**
 * Print the version of PRoot.
 */
void print_version(const Cli *cli)
{
	printf("%s %s\n\n", cli->logo, cli->version);
	printf("built-in accelerators: process_vm = %s, seccomp_filter = %s\n",
#if defined(HAVE_PROCESS_VM)
		"yes",
#else
		"no",
#endif
#if defined(HAVE_SECCOMP_FILTER)
		"yes"
#else
		"no"
#endif
		);
}

static void print_execve_help(const Tracee *tracee, const char *argv0)
{
	notice(tracee, WARNING, SYSTEM, "execve(\"%s\")", argv0);
	notice(tracee, INFO, USER, "possible causes:\n"
"  * <program> is a script but its interpreter (eg. /bin/sh) was not found;\n"
"  * <program> is an ELF but its interpreter (eg. ld-linux.so) was not found;\n"
"  * <program> is a foreign binary but no <qemu> was specified;\n"
"  * <qemu> does not work correctly (if specified).");
}

static void print_error_separator(const Tracee *tracee, const Argument *argument)
{
	if (argument->separator == '\0')
		notice(tracee, ERROR, USER, "option '%s' expects no value.", argument->name);
	else
		notice(tracee, ERROR, USER,
			"option '%s' and its value must be separated by '%c'.",
			argument->name, argument->separator);
}

static void print_argv(const Tracee *tracee, const char *prompt, char **argv)
{
	char string[ARG_MAX] = "";
	size_t i;

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

	notice(tracee, INFO, USER, "%s", string);
}

static void print_config(Tracee *tracee)
{
	assert(tracee != NULL);

	if (tracee->verbose <= 0)
		return;

	if (tracee->qemu)
		notice(tracee, INFO, USER, "host rootfs = %s", HOST_ROOTFS);

	if (tracee->glue)
		notice(tracee, INFO, USER, "glue rootfs = %s", tracee->glue);

	notice(tracee, INFO, USER, "exe = %s", tracee->exe);
	print_argv(tracee, "command", tracee->cmdline);
	print_argv(tracee, "qemu", tracee->qemu);
	notice(tracee, INFO, USER, "initial cwd = %s", tracee->fs->cwd);
	notice(tracee, INFO, USER, "verbose level = %d", tracee->verbose);

	notify_extensions(tracee, PRINT_CONFIG, 0, 0);
}

/**
 * Initialize @tracee's current working directory.  This function
 * returns -1 if an error occurred, otherwise 0.
 */
static int initialize_cwd(Tracee *tracee)
{
	char path2[PATH_MAX];
	char path[PATH_MAX];
	int status;

	/* Compute the base directory.  */
	if (tracee->fs->cwd[0] != '/') {
		status = getcwd2(tracee->reconf.tracee, path);
		if (status < 0) {
			notice(tracee, ERROR, INTERNAL, "getcwd: %s", strerror(-status));
			return -1;
		}
	}
	else
		strcpy(path, "/");

	/* The ending "." ensures canonicalize() will report an error
	 * if tracee->fs->cwd does not exist or if it is not a
	 * directory.  */
	status = join_paths(3, path2, path, tracee->fs->cwd, ".");
	if (status < 0) {
		notice(tracee, ERROR, INTERNAL, "getcwd: %s", strerror(-status));
		return -1;
	}

	/* Initiale state for canonicalization.  */
	strcpy(path, "/");

	status = canonicalize(tracee, path2, true, path, 0);
	if (status < 0) {
		notice(tracee, WARNING, USER, "can't chdir(\"%s\") in the guest rootfs: %s",
			path2, strerror(-status));
		notice(tracee, INFO, USER, "default working directory is now \"/\"");
		strcpy(path, "/");
	}
	chop_finality(path);

	/* Replace with the canonicalized working directory.  */
	TALLOC_FREE(tracee->fs->cwd);
	tracee->fs->cwd = talloc_strdup(tracee->fs, path);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");

	return 0;
}

/**
 * Initialize @tracee->exe and @tracee->cmdline from @cmdline.
 */
static int initialize_command(Tracee *tracee, char *const *cmdline)
{
	char path[PATH_MAX];
	int status;
	int i;

	/* How many arguments?  */
	for (i = 0; cmdline[i] != NULL; i++)
		;

	tracee->cmdline = talloc_zero_array(tracee, char *, i + 1);
	if (tracee->cmdline == NULL) {
		notice(tracee, ERROR, INTERNAL, "talloc_zero_array() failed");
		return -1;
	}
	talloc_set_name_const(tracee->cmdline, "@cmdline");

	for (i = 0; cmdline[i] != NULL; i++)
		tracee->cmdline[i] = cmdline[i];

	/* Resolve the full guest path to tracee->cmdline[0].  */
	status = which(tracee, tracee->reconf.paths, path, tracee->cmdline[0]);
	if (status < 0)
		return -1;

	status = detranslate_path(tracee, path, NULL);
	if (status < 0)
		return -1;

	tracee->exe = talloc_strdup(tracee, path);
	if (tracee->exe == NULL)
		return -1;
	talloc_set_name_const(tracee->exe, "$exe");

	return 0;
}

/**
 * Configure @tracee according to the command-line arguments stored in
 * @argv[].  This function returns -1 if an error occured, otherwise
 * 0.
 */
int parse_config(Tracee *tracee, size_t argc, char *argv[])
{
	char *const default_command[] = { "/bin/sh", NULL };
	option_handler_t handler = NULL;
	const Option *options;
	size_t i, j, k;
	int status;
	const Cli *cli;

	/* As of now, only the PRoot CLI is supported, the code below
	 * is just a mock-up.  */
	if (strncasecmp(basename(argv[0]), "care", strlen("care")) == 0)
		cli = get_proot_cli(tracee->ctx);
	else
		cli = get_proot_cli(tracee->ctx);
	if (cli == NULL)
		return -1;
	tracee->tool_name = cli->name;

	if (argc == 1) {
		print_usage(tracee, cli, false);
		return -1;
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		/* The current argument is the value of a short option.  */
		if (handler != NULL) {
			status = handler(tracee, cli, arg);
			if (status < 0)
				return status;
			handler = NULL;
			continue;
		}

		if (arg[0] != '-')
			break; /* End of PRoot options. */

		options = cli->options;
		for (j = 0; options[j].class != NULL; j++) {
			const Option *option = &options[j];

			/* A given option has several aliases.  */
			for (k = 0; ; k++) {
				const Argument *argument;
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
					status = option->handler(tracee, cli, NULL);
					if (status < 0)
						return -1;
					goto known_option;
				}

				/* Value coalesced with to its option.  */
				if (argument->separator == arg[length]) {
					assert(strlen(arg) >= length);
					status = option->handler(tracee, cli, &arg[length + 1]);
					if (status < 0)
						return -1;
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

		notice(tracee, ERROR, USER, "unknown option '%s'.", arg);
		return -1;

	known_option:
		if (handler != NULL && i == argc - 1) {
			notice(tracee, ERROR, USER, "missing value for option '%s'.", arg);
			return -1;
		}
	}

#define HOOK_CONFIG(callback)						\
	do {								\
		if (cli->callback != NULL) {				\
			status = cli->callback(tracee, cli, argc, argv, i); \
			if (status < 0)					\
				return -1;				\
			i = status;					\
		}							\
	} while (0)

	HOOK_CONFIG(pre_initialize_bindings);

	/* The guest rootfs is now known: bindings specified by the
	 * user (tracee->bindings.user) can be canonicalized.  */
	status = initialize_bindings(tracee);
	if (status < 0)
		return -1;

	HOOK_CONFIG(post_initialize_bindings);
	HOOK_CONFIG(pre_initialize_cwd);

	/* Bindings are now installed (tracee->bindings.guest &
	 * tracee->bindings.host): the current working directory can
	 * be canonicalized.  */
	status = initialize_cwd(tracee);
	if (status < 0)
		return -1;

	HOOK_CONFIG(post_initialize_cwd);
	HOOK_CONFIG(pre_initialize_command);

	/* Bindings are now installed and the current working
	 * directory is canonicalized: resolve path to @tracee->exe
	 * and configure @tracee->cmdline.  */
	status = initialize_command(tracee, i < argc ? &argv[i] : default_command);
	if (status < 0)
		return -1;

	HOOK_CONFIG(post_initialize_command);
#undef HOOK_CONFIG

	print_config(tracee);

	return 0;
}

bool exit_failure = true;

int main(int argc, char *argv[])
{
	Tracee *tracee;
	int status;

	/* Configure the memory allocator.  */
	talloc_enable_leak_report();

#if defined(TALLOC_VERSION_MAJOR) && TALLOC_VERSION_MAJOR >= 2
	talloc_set_log_stderr();
#endif

	/* Pre-create the first tracee (pid == 0).  */
	tracee = get_tracee(NULL, 0, true);
	if (tracee == NULL)
		goto error;
	tracee->pid = getpid();

	/* Pre-configure the first tracee.  */
	status = parse_config(tracee, argc, argv);
	if (status < 0)
		goto error;

	/* Start the first tracee.  */
	status = launch_process(tracee);
	if (status < 0) {
		print_execve_help(tracee, tracee->exe);
		goto error;
	}

	/* Start tracing the first tracee and all its children.  */
	exit(event_loop());

error:
	TALLOC_FREE(tracee);

	if (exit_failure) {
		fprintf(stderr, "fatal error: see `%1$s --help` or `man %1$s`.\n",
			basename(argv[0]));
		exit(EXIT_FAILURE);
	}
	else
		exit(EXIT_SUCCESS);
}

/**
 * Convert @value into an integer, then put the result into
 * *@variable.  This function prints a warning and returns -1 if a
 * conversion error occured, otherwise it returns 0.
 */
int parse_integer_option(const Tracee *tracee, int *variable, const char *value, const char *option)
{
	char *end_ptr = NULL;

	errno = 0;
	*variable = strtol(value, &end_ptr, 10);
	if (errno != 0 || end_ptr == value) {
		notice(tracee, ERROR, USER, "option `%s` expects an integer value.", option);
		return -1;
	}

	return 0;
}
