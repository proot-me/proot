/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of WioM.
 *
 * Copyright (C) 2014 STMicroelectronics
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

#include <unistd.h>	/* getcwd(2), */
#include <limits.h> 	/* PATH_MAX, */
#include <sys/queue.h> 	/* SIMPLEQ_*, */
#include <stdio.h> 	/* strerror(3), */

#include "cli/wiom.h"
#include "cli/cli.h"
#include "cli/note.h"
#include "tracee/tracee.h"
#include "extension/extension.h"
#include "extension/wiom/wiom.h"
#include "path/binding.h"
#include "path/path.h"
#include "attribute.h"

static int handle_option_i(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-i option not yet implemented");
	return -1;
}

static int handle_option_o(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-o option not yet implemented");
	return -1;
}

static int handle_option_f(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-f option not yet implemented");
	return -1;
}

static int handle_option_p(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-p option not yet implemented");
	return -1;
}

static int add_path(Tracee *tracee, Options *options, const char *path, bool masked)
{
	List *list;
	Item *item;

	list = (masked
		? options->masked_paths
		: options->unmasked_paths);

	if (list == NULL) {
		list = talloc(options, List);
		if (list == NULL) {
			note(tracee, ERROR, SYSTEM, "not enough memory");
			return -1;
		}

		SIMPLEQ_INIT(list);

		if (masked)
			options->masked_paths = list;
		else
			options->unmasked_paths = list;
	}

	item = talloc(list, Item);
	if (item == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	item->load = talloc_strdup(item, path);
	if (item->load == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	SIMPLEQ_INSERT_HEAD(list, item, link);

	return 0;
}

static int handle_option_m(Tracee *tracee UNUSED, const Cli *cli, const char *value)
{
	return add_path(tracee, talloc_get_type_abort(cli->private, Options), value, true);
}

static int handle_option_M(Tracee *tracee UNUSED, const Cli *cli UNUSED, const char *value UNUSED)
{
	return add_path(tracee, talloc_get_type_abort(cli->private, Options), value, false);
}

static int handle_option_r(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-R option not yet implemented");
	return -1;
}

static int handle_option_c(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-C option not yet implemented");
	return -1;
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

static int handle_option_V(Tracee *tracee UNUSED, const Cli *cli, const char *value UNUSED)
{
	print_version(cli);
	printf("\n%s\n", cli->colophon);
	fflush(stdout);

	exit_failure = false;
	return -1;
}

static int handle_option_h(Tracee *tracee UNUSED, const Cli *cli UNUSED, const char *value UNUSED)
{
	print_usage(tracee, cli, true);
	exit_failure = false;
	return -1;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot/WioM but
 * that are not specifiable on the command line.
 */
static int pre_initialize_bindings(Tracee *tracee UNUSED, const Cli *cli UNUSED,
				size_t argc UNUSED, char *const argv[] UNUSED, size_t cursor)
{
	char path[PATH_MAX];
	Binding *binding;

	/* Initialize @tracee->fs->cwd (required by PRoot).  */
	if (getcwd(path, PATH_MAX) == NULL) {
		note(tracee, ERROR, SYSTEM, "can't get current working directory");
		return -1;
	}

	tracee->fs->cwd = talloc_strdup(tracee->fs, path);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");

	/* Initialize @tracee's root (required by PRoot).  */
	binding = new_binding(tracee, "/", "/", true);
	if (binding == NULL)
		return -1;

	return cursor;
}

static int canonicalize_paths(Tracee *tracee, List *list)
{
	char path[PATH_MAX];
	Item *item;
	int status;

	if (list == NULL)
		return 0;

	SIMPLEQ_FOREACH(item, list, link) {
		status = realpath2(tracee, path, item->load, false);
		if (status < 0) {
			note(tracee, ERROR, SYSTEM, "can't canonicalize '%s': %s",
				(char *) item->load, strerror(-status));
			return -1;
		}

		TALLOC_FREE(item->load);
		item->load = talloc_strdup(item, path);
		if (item->load == NULL) {
			note(tracee, ERROR, SYSTEM, "not enough memory");
			return -1;
		}
	}

	return 0;
}

/**
 * Initialize WioM extensions.
 */
static int post_initialize_bindings(Tracee *tracee UNUSED, const Cli *cli UNUSED,
			       size_t argc UNUSED, char *const argv[] UNUSED, size_t cursor)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	int status;

	status = canonicalize_paths(tracee, options->masked_paths);
	if (status < 0)
		return -1;

	status = canonicalize_paths(tracee, options->unmasked_paths);
	if (status < 0)
		return -1;

	status = initialize_extension(tracee, wiom_callback, (void *) options);
	if (status < 0) {
		note(tracee, WARNING, INTERNAL, "can't initialize the wiom extension");
		return -1;
	}

	return cursor;
}

const Cli *get_wiom_cli(TALLOC_CTX *context)
{
	Options *options;

	global_tool_name = wiom_cli.name;

	options = talloc_zero(context, Options);
	if (options == NULL)
		return NULL;

	wiom_cli.private = options;
	return &wiom_cli;
}
