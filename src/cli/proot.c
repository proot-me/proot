/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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
#include <stdio.h>     /* printf(3), fflush(3), */
#include <unistd.h>    /* write(2), */

#include "cli/cli.h"
#include "cli/note.h"
#include "extension/extension.h"
#include "path/binding.h"
#include "attribute.h"

/* These should be included last.  */
#include "build.h"
#include "cli/proot.h"

static int handle_option_r(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	Binding *binding;

	/* ``chroot $PATH`` is semantically equivalent to ``mount
	 * --bind $PATH /``.  */
	binding = new_binding(tracee, value, "/", true);
	if (binding == NULL)
		return -1;

	return 0;
}

static int handle_option_b(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	char *host;
	char *guest;

	host = talloc_strdup(tracee->ctx, value);
	if (host == NULL) {
		note(tracee, ERROR, INTERNAL, "can't allocate memory");
		return -1;
	}

	guest = strchr(host, ':');
	if (guest != NULL) {
		*guest = '\0';
		guest++;
	}

	new_binding(tracee, host, guest, true);
	return 0;
}

static int handle_option_q(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	const char *ptr;
	size_t nb_args;
	bool last;
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
	do {
		const void *start;
		const void *end;
		last = true;

		/* Keep consecutive non-space characters.  */
		start = ptr;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		end = ptr;

		/* End-of-string ?  */
		if (*ptr == '\0')
			goto next;

		/* Remove consecutive space separators.  */
		while (*ptr == ' ' && *ptr != '\0')
			ptr++;

		/* End-of-string ?  */
		if (*ptr == '\0')
			goto next;

		last = false;
	next:
		tracee->qemu[i] = talloc_strndup(tracee->qemu, start, end - start);
		if (tracee->qemu[i] == NULL)
			return -1;
		i++;
	} while (!last);
	assert(i == nb_args);

	new_binding(tracee, "/", HOST_ROOTFS, true);
	new_binding(tracee, "/dev/null", "/etc/ld.so.preload", false);

	return 0;
}

static int handle_option_mixed_mode(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	tracee->mixed_mode = value;
	return 0;
}

static int handle_option_w(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	tracee->fs->cwd = talloc_strdup(tracee->fs, value);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");
	return 0;
}

static int handle_option_k(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	void *extension;
	int status;

	extension = get_extension(tracee, kompat_callback);
	if (extension != NULL) {
		note(tracee, WARNING, USER, "option -k was already specified");
		note(tracee, INFO, USER, "only the last -k option is enabled");
		TALLOC_FREE(extension);
	}

	status = initialize_extension(tracee, kompat_callback, value);
	if (status < 0)
		note(tracee, WARNING, INTERNAL, "option \"-k %s\" discarded", value);

	return 0;
}

static int handle_option_i(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	void *extension;

	extension = get_extension(tracee, fake_id0_callback);
	if (extension != NULL) {
		note(tracee, WARNING, USER, "option -i/-0/-S was already specified");
		note(tracee, INFO, USER, "only the last -i/-0/-S option is enabled");
		TALLOC_FREE(extension);
	}

	(void) initialize_extension(tracee, fake_id0_callback, value);
	return 0;
}

static int handle_option_0(Tracee *tracee, const Cli *cli, const char *value UNUSED)
{
	return handle_option_i(tracee, cli, "0:0");
}

static int handle_option_kill_on_exit(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	tracee->killall_on_exit = true;
	return 0;
}

static int handle_option_v(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	int status;

	status = parse_integer_option(tracee, &tracee->verbose, value, "-v");
	if (status < 0)
		return status;

	global_verbose_level = tracee->verbose;
	return 0;
}

extern unsigned char WEAK _binary_licenses_start;
extern unsigned char WEAK _binary_licenses_end;

static int handle_option_V(Tracee *tracee UNUSED, const Cli *cli, const char *value UNUSED)
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

static int handle_option_h(Tracee *tracee, const Cli *cli, const char *value UNUSED)
{
	print_usage(tracee, cli, true);
	exit_failure = false;
	return -1;
}

static void new_bindings(Tracee *tracee, const char *bindings[], const char *value)
{
	int i;

	for (i = 0; bindings[i] != NULL; i++) {
		const char *path;

		path = (strcmp(bindings[i], "*path*") != 0
			? expand_front_variable(tracee->ctx, bindings[i])
			: value);

		new_binding(tracee, path, NULL, false);
	}
}

static int handle_option_R(Tracee *tracee, const Cli *cli, const char *value)
{
	int status;

	status = handle_option_r(tracee, cli, value);
	if (status < 0)
		return status;

	new_bindings(tracee, recommended_bindings, value);

	return 0;
}

static int handle_option_S(Tracee *tracee, const Cli *cli, const char *value)
{
	int status;

	status = handle_option_0(tracee, cli, value);
	if (status < 0)
		return status;

	status = handle_option_r(tracee, cli, value);
	if (status < 0)
		return status;

	new_bindings(tracee, recommended_su_bindings, value);

	return 0;
}

static int handle_option_p(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	int status = 0;
	char *port_in;
	char *port_out;

	port_in = talloc_strdup(tracee->ctx, value);
	if (port_in == NULL) {
		note(tracee, ERROR, INTERNAL, "can't allocate memory");
		return -1;
	}

	port_out = strchr(port_in, ':');
	if (port_out != NULL) {
		*port_out = '\0';
		port_out++;
	}

	if(global_portmap_extension == NULL)
		status = initialize_extension(tracee, portmap_callback, value);
	if(status < 0)
		return status;

	status = add_portmap_entry(atoi(port_in), atoi(port_out));

	return status;
}

static int handle_option_n(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	int status = 0;

	if(global_portmap_extension == NULL)
		status = initialize_extension(tracee, portmap_callback, value);
	if(status < 0)
		return status;

	status = activate_netcoop_mode();

	return status;
}

#ifdef HAVE_PYTHON_EXTENSION
static int handle_option_P(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	(void) initialize_extension(tracee, python_callback, value);
	return 0;
}
#endif

static int handle_option_l(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	return initialize_extension(tracee, link2symlink_callback, NULL);
}

/**
 * Initialize @tracee->qemu.
 */
static int post_initialize_exe(Tracee *tracee, const Cli *cli UNUSED,
			size_t argc UNUSED, char *const argv[] UNUSED, size_t cursor UNUSED)
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

	return 0;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot but that
 * are not required on the command line, i.e.  "-w" and "-r".
 */
static int pre_initialize_bindings(Tracee *tracee, const Cli *cli,
			size_t argc UNUSED, char *const argv[] UNUSED, size_t cursor)
{
	int status;

	/* Default to "." if no CWD were specified.  */
	if (tracee->fs->cwd == NULL) {
		status = handle_option_w(tracee, cli, ".");
		if (status < 0)
			return -1;
	}

	 /* The default guest rootfs is "/" if none was specified.  */
	if (get_root(tracee) == NULL) {
		status = handle_option_r(tracee, cli, "/");
		if (status < 0)
			return -1;
	}

	return cursor;
}

const Cli *get_proot_cli(TALLOC_CTX *context UNUSED)
{
	global_tool_name = proot_cli.name;
	return &proot_cli;
}
