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

#include <string.h>    /* str*(3), */
#include <assert.h>    /* assert(3), */
#include <sys/types.h> /* stat(2), */
#include <sys/stat.h>  /* stat(2), */
#include <unistd.h>    /* stat(2), */
#include <stdio.h>     /* printf(3), fflush(3), */

#include "cli/cli.h"
#include "cli/notice.h"
#include "extension/extension.h"
#include "path/binding.h"
#include "attribute.h"

/* These should be included last.  */
#include "build.h"
#include "cli/proot.h"

static int handle_option_r(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	Binding *binding;

	/* ``chroot $PATH`` is semantically equivalent to ``mount
	 * --bind $PATH /``.  */
	binding = new_binding(tracee, value, "/", true);
	if (binding == NULL)
		return -1;

	return 0;
}

static int handle_option_b(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	char *ptr = strchr(value, ':');
	if (ptr != NULL) {
		*ptr = '\0';
		ptr++;
	}

	new_binding(tracee, value, ptr, true);
	return 0;
}

static int handle_option_q(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	size_t nb_args;
	char *ptr;
	size_t i;

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

static int handle_option_w(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	tracee->fs->cwd = talloc_strdup(tracee->fs, value);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");
	return 0;
}

static int handle_option_k(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	int status;

	status = initialize_extension(tracee, kompat_callback, value);
	if (status < 0)
		notice(tracee, WARNING, INTERNAL, "option \"-k %s\" discarded", value);

	return 0;
}

static int handle_option_0(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	(void) initialize_extension(tracee, fake_id0_callback, value);
	return 0;
}

static int handle_option_v(Tracee *tracee, const Cli *cli UNUSED, char *value)
{
	return parse_integer_option(tracee, &tracee->verbose, value, "-v");
}

extern char __attribute__((weak)) _binary_licenses_start;
extern char __attribute__((weak)) _binary_licenses_end;

static int handle_option_V(Tracee *tracee UNUSED, const Cli *cli, char *value UNUSED)
{
	size_t size;

	print_version(cli);
	printf("\n%s\n", cli->colophon);
	fflush(stdout);

	size = &_binary_licenses_end - &_binary_licenses_start;
	if (size > 0)
		write(1, &_binary_licenses_start, size);

	exit_failure = false;
	return -1;
}

static int handle_option_h(Tracee *tracee, const Cli *cli, char *value UNUSED)
{
	print_usage(tracee, cli, true);
	exit_failure = false;
	return -1;
}

static int handle_option_R(Tracee *tracee, const Cli *cli, char *value)
{
	int status;
	int i;

	status = handle_option_r(tracee, cli, value);
	if (status < 0)
		return status;

	for (i = 0; recommended_bindings[i] != NULL; i++) {
		const char *path;

		path = (strcmp(recommended_bindings[i], "*path*") != 0
			? expand_front_variable(tracee->ctx, recommended_bindings[i])
			: value);

		new_binding(tracee, path, NULL, false);
	}
	return 0;
}

static int handle_option_B(Tracee *tracee, const Cli *cli UNUSED, char *value UNUSED)
{
	int i;

	notice(tracee, INFO, USER,
		"option '-B' (and '-Q') is obsolete, "
		"use '-R path/to/rootfs' (maybe with '-q') instead.");

	for (i = 0; recommended_bindings[i] != NULL; i++)
		new_binding(tracee, expand_front_variable(tracee->ctx, recommended_bindings[i]),
			NULL, false);

	return 0;
}

static int handle_option_Q(Tracee *tracee, const Cli *cli, char *value)
{
	int status;

	status = handle_option_q(tracee, cli, value);
	if (status < 0)
		return status;

	status = handle_option_B(tracee, cli, NULL);
	if (status < 0)
		return status;

	return 0;
}

/**
 * Initialize @tracee->qemu.
 */
static int post_initialize_command(Tracee *tracee, const Cli *cli UNUSED,
			size_t argc UNUSED, char *const *argv UNUSED, size_t cursor UNUSED)
{
	char path[PATH_MAX];
	int status;

	/* Nothing else to do ?  */
	if (tracee->qemu == NULL)
		return 0;

	/* Resolve the full guest path to tracee->qemu[0].  */
	status = which(tracee->reconf.tracee, tracee->reconf.paths, path, tracee->qemu[0]);
	if (status < 0)
		return -1;

	/* Actually tracee->qemu[0] has to be a host path from the tracee's
	 * point-of-view, not from the PRoot's point-of-view.  See
	 * translate_execve() for details.  */
	if (tracee->reconf.tracee != NULL) {
		status = detranslate_path(tracee->reconf.tracee, path, NULL);
		if (status < 0)
			return -1;
	}

	tracee->qemu[0] = talloc_strdup(tracee->qemu, path);
	if (tracee->qemu[0] == NULL)
		return -1;

	/**
	 * There's a bug when using the ELF interpreter as a loader (as PRoot
	 * does) on PIE programs that uses constructors (typically QEMU v1.1+).
	 * In this case, constructors are called twice as you can see on the
	 * test below:
	 *
	 *     $ cat test.c
	 *     static void __attribute__((constructor)) init(void) { puts("OK"); }
	 *     int main() { return 0; }
	 *
	 *     $ gcc -fPIC -pie test.c -o test
	 *     $ ./test
	 *     OK
	 *
	 *     $ /lib64/ld-linux-x86-64.so.2 ./test
	 *     OK
	 *     OK
	 *
	 * Actually, PRoot doesn't have to use the ELF interpreter as a loader
	 * if QEMU isn't nested.  When QEMU is nested (sub reconfiguration), the
	 * user has to use either a version of QEMU prior v1.1 or a version of
	 * QEMU compiled with the --disable-pie option.
	 */
	tracee->qemu_pie_workaround = (tracee->reconf.tracee == NULL);
	return 0;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot but that
 * are not required on the command line, i.e.  "-w" and "-r".
 */
static int pre_initialize_bindings(Tracee *tracee, const Cli *cli,
			size_t argc UNUSED, char *const *argv, size_t cursor)
{
	int status;

	/* Default to "." if no CWD were specified.  */
	if (tracee->fs->cwd == NULL) {
		status = handle_option_w(tracee, cli, ".");
		if (status < 0)
			return -1;
	}

	/* When no guest rootfs were specified: if the first bare
	 * option is a directory, then the old command-line interface
	 * (similar to the chroot one) is expected.  Otherwise this is
	 * the new command-line interface where the default guest
	 * rootfs is "/".  */
	if (get_root(tracee) == NULL) {
		char path[PATH_MAX];
		struct stat buf;

		if (argv[cursor] != NULL
		    && realpath2(tracee->reconf.tracee, path, argv[cursor], true) == 0
		    && stat(path, &buf) == 0
		    && S_ISDIR(buf.st_mode)) {
			notice(tracee, INFO, USER,
				"neither `-r` or `-R` were specified, assuming"
				" '%s' is the new root file-system.", argv[cursor]);
			status = handle_option_r(tracee, cli, argv[cursor]);
			cursor++;
		}
		else
			status = handle_option_r(tracee, cli, "/");
		if (status < 0)
			return -1;
	}

	return cursor;
}

const Cli *get_proot_cli(TALLOC_CTX *context UNUSED)
{
	return &proot_cli;
}
