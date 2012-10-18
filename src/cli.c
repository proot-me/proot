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
#include "path/canon.h"
#include "path/binding.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "extension/extension.h"
#include "build.h"

static int handle_option_r(Tracee *tracee, char *value)
{
	Binding *binding;

	/* ``chroot $PATH`` is semantically equivalent to ``mount
	 * --bind $PATH /``.  */
	binding = new_binding(tracee, value, "/", true);
	if (binding == NULL)
		return -1;

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

static int handle_option_q(Tracee *tracee, char *value)
{
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
	if (tracee->qemu == NULL)
		return -1;
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

	new_binding(tracee, "/", HOST_ROOTFS, true);
	new_binding(tracee, "/dev/null", "/etc/ld.so.preload", false);

	return 0;
}

static int handle_option_w(Tracee *tracee, char *value)
{
	tracee->fs->cwd = talloc_strdup(tracee->fs, value);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");
	return 0;
}

static int handle_option_k(Tracee *tracee, char *value)
{
	int status;

	status = initialize_extension(tracee, kompat_callback, value);
	if (status < 0)
		notice(tracee, WARNING, INTERNAL, "option \"-k %s\" discarded", value);

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
	tracee->verbose = strtol(value, &end_ptr, 10);
	if (errno != 0 || end_ptr == value) {
		notice(tracee, ERROR, USER, "option `-v` expects an integer value.");
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
		printf("%s\n", colophon);
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

static void print_error_separator(const Tracee *tracee, Argument *argument)
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

	/* Default to "." if none were specified.  */
	if (tracee->fs->cwd == NULL) {
		status = handle_option_w(tracee, ".");
		if (status < 0)
			return -1;
	}

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
 * Initialize @tracee->exe and @tracee->cmdline from @cmdline, and resolve the
 * full host path to @command@tracee->qemu[0].
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

	/* Nothing else to do ?  */
	if (tracee->qemu == NULL)
		return 0;

	/* Resolve the full guest path to tracee->qemu[0].  */
	status = which(tracee->reconf.tracee, tracee->reconf.paths, path, tracee->qemu[0]);
	if (status < 0)
		return -1;

	tracee->qemu[0] = talloc_strdup(tracee->qemu, path);
	if (tracee->qemu[0] == NULL)
		return -1;

	return 0;
}

static char *default_command[] = { "/bin/sh", NULL };

/**
 * Configure @tracee according to the command-line arguments stored in
 * @argv[].  This function returns -1 if an error occured, otherwise
 * 0.
 */
int parse_config(Tracee *tracee, int argc, char *argv[])
{
	option_handler_t handler = NULL;
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
						return -1;
					goto known_option;
				}

				/* Value coalesced with to its option.  */
				if (argument->separator == arg[length]) {
					assert(strlen(arg) >= length);
					status = option->handler(tracee, &arg[length + 1]);
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

	/* When no guest rootfs were specified: if the first bare
	 * option is a directory, then the old command-line interface
	 * (similar to the chroot one) is expected.  Otherwise this is
	 * the new command-line interface where the default guest
	 * rootfs is "/".  */
	if (get_root(tracee) == NULL) {
		char path[PATH_MAX];
		struct stat buf;

		if (argv[i] != NULL
		    && realpath2(tracee->reconf.tracee, path, argv[i], true) == 0
		    && stat(path, &buf) == 0
		    && S_ISDIR(buf.st_mode)) {
			status = handle_option_r(tracee, argv[i]);
			i++;
		}
		else
			status = handle_option_r(tracee, "/");
		if (status < 0)
			return -1;
	}

	/* The guest rootfs is now known: bindings specified by the
	 * user (tracee->bindings.user) can be canonicalized.  */
	status = initialize_bindings(tracee);
	if (status < 0)
		return -1;

	/* Bindings are now installed (tracee->bindings.guest &
	 * tracee->bindings.host): the current working directory can
	 * be canonicalized.  */
	status = initialize_cwd(tracee);
	if (status < 0)
		return -1;

	/* Bindings are now installed and the current working directory is
	 * canonicalized: resolve path to @tracee->exe and @tracee->qemu.  */
	status = initialize_command(tracee, i < argc ? &argv[i] : default_command);
	if (status < 0)
		return -1;

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
		fprintf(stderr, "proot error: see `proot --help` or `man proot`.\n");
		exit(EXIT_FAILURE);
	}
	else
		exit(EXIT_SUCCESS);
}
