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
#include <sys/queue.h> 	/* SIMPLEQ_*, LIST_*, */
#include <stdio.h> 	/* strerror(3), */
#include <assert.h> 	/* assert(3), */
#include <string.h> 	/* str*, */

#include "cli/wiom.h"
#include "cli/cli.h"
#include "cli/note.h"
#include "tracee/tracee.h"
#include "extension/extension.h"
#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "extension/wiom/event.h"
#include "path/binding.h"
#include "path/path.h"
#include "attribute.h"

static int handle_option_i(Tracee *tracee, const Cli *cli, const char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);

	if (options->input_fd != -1) {
		note(tracee, WARNING, USER, "\"-i %s\" overrides previous choice", value);
		close(options->input_fd);
	}

	options->input_fd = open(value, O_RDONLY);
	if (options->input_fd < 0) {
		note(tracee, ERROR, SYSTEM, "can't open %s", value);
		return -1;
	}

	return 0;
}

static int handle_option_o(Tracee *tracee, const Cli *cli, const char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);

	if (options->output.fd != -1) {
		note(tracee, WARNING, USER, "\"-o %s\" overrides previous choice", value);
		close(options->output.fd);
	}

	options->output.fd = open(value, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (options->output.fd < 0) {
		note(tracee, ERROR, SYSTEM, "can't open %s", value);
		return -1;
	}

	return 0;
}

static int handle_option_f(Tracee *tracee, const Cli *cli, const char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);

	if (options->output.format != NONE)
		note(tracee, WARNING, USER, "\"-f %s\" overrides previous choice", value);

	if (strcmp(value, "binary") == 0)
		options->output.format = BINARY;
	else if (strcmp(value, "raw") == 0)
		options->output.format = RAW;
	else if (strcmp(value, "fs_state") == 0)
		options->output.format = FS_STATE;
	else {
		options->output.format = NONE;
		note(tracee, WARNING, USER, "\"-f %s\" not supported", value);
	}

	return 0;
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
		? options->paths.masked
		: options->paths.unmasked);

	if (list == NULL) {
		list = talloc(options, List);
		if (list == NULL) {
			note(tracee, ERROR, SYSTEM, "not enough memory");
			return -1;
		}

		SIMPLEQ_INIT(list);

		if (masked)
			options->paths.masked = list;
		else
			options->paths.unmasked = list;
	}

	item = talloc(list, Item);
	if (item == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	item->payload = talloc_strdup(item, path);
	if (item->payload == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	SIMPLEQ_INSERT_HEAD(list, item, link);

	return 0;
}

static int handle_option_m(Tracee *tracee, const Cli *cli, const char *value)
{
	return add_path(tracee, talloc_get_type_abort(cli->private, Options), value, true);
}

static int handle_option_M(Tracee *tracee, const Cli *cli, const char *value)
{
	return add_path(tracee, talloc_get_type_abort(cli->private, Options), value, false);
}

static int handle_option_a(Tracee *tracee, const Cli *cli, const char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	const char *cursor;

	#define ACTION(name) #name,
	static const char *known_actions[] = {
		#include "extension/wiom/actions.list"
		NULL,
	};
	#undef ACTION

	options->actions.filter = 0;

	cursor = value;
	while (1) {
		const char *old_cursor = cursor;
		size_t size;
		bool known;
		size_t i;

		cursor = strchrnul(old_cursor, ',');
		size = cursor - old_cursor;

		for (known = false, i = 0; known_actions[i] != NULL; i++) {
			if (strncasecmp(known_actions[i], old_cursor, size) != 0)
				continue;

			if (GET_ACTION_BIT(options, i) != 0)
				note(tracee, WARNING, USER,
					"action '%s' already set", known_actions[i]);

			SET_ACTION_BIT(options, i);

			puts(known_actions[i]);
			known = true;
		}

		if (!known) {
			note(tracee, WARNING, USER, "unknown action '%s'", old_cursor);
			note(tracee, INFO, USER, "Supported actions are:");
			for (known = false, i = 0; known_actions[i] != NULL; i++)
				note(tracee, INFO, USER, "\t%s", known_actions[i]);
			return -1;
		}

		if (cursor[0] == '\0')
			break;

		cursor++;
	}

	return 0;
}

static int handle_option_c(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	note(tracee, ERROR, INTERNAL, "-c option not yet implemented");
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

static int handle_option_h(Tracee *tracee, const Cli *cli, const char *value UNUSED)
{
	print_usage(tracee, cli, true);
	exit_failure = false;
	return -1;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot/WioM but
 * that are not specifiable on the command line.
 */
static int pre_initialize_bindings(Tracee *tracee, const Cli *cli UNUSED,
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
		status = realpath2(tracee, path, item->payload, false);
		if (status < 0) {
			note(tracee, ERROR, SYSTEM, "can't canonicalize '%s': %s",
				(char *) item->payload, strerror(-status));
			return -1;
		}

		TALLOC_FREE(item->payload);
		item->payload = talloc_strdup(item, path);
		if (item->payload == NULL) {
			note(tracee, ERROR, SYSTEM, "not enough memory");
			return -1;
		}
	}

	return 0;
}

/**
 * Initialize WioM extensions.
 */
static int post_initialize_bindings(Tracee *tracee, const Cli *cli,
			       size_t argc UNUSED, char *const argv[] UNUSED, size_t cursor)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	int status;

	if (options->output.format == NONE) {
		options->output.format = (options->output.fd != -1
					? BINARY
					: FS_STATE);
	}

	if (options->output.fd == -1)
		options->output.fd = 1; /* stdout */

	status = canonicalize_paths(tracee, options->paths.masked);
	if (status < 0)
		return -1;

	status = canonicalize_paths(tracee, options->paths.unmasked);
	if (status < 0)
		return -1;

	status = initialize_extension(tracee, wiom_callback, (void *) options);
	if (status < 0) {
		note(tracee, WARNING, INTERNAL, "can't initialize the wiom extension");
		return -1;
	}

	return cursor;
}

/**
 * Replay events if -i option was specified, instead of executing a
 * command.
 */
static int pre_initialize_exe(Tracee *tracee, const Cli *cli,
			size_t argc, char *const argv[] UNUSED, size_t cursor)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	Config *config = NULL;
	Extension *extension;
	int status;

	if (options->input_fd == -1)
		return cursor;

	if (cursor != argc)
		note(tracee, WARNING, USER,
			"both '-i' option and a command are specified; these are mutually"
			" exclusive thus this latter is discared.");

	/* Search for WioM configuration.  */
	LIST_FOREACH(extension, tracee->extensions, link) {
		if (extension->callback == wiom_callback) {
			config = extension->config;
			break;
		}
	}
	assert(config != NULL);

	status = replay_events_binary(tracee->ctx, config->shared);
	if (status < 0)
		return -1;

	exit_failure = false;
	return -1;
}

const Cli *get_wiom_cli(TALLOC_CTX *context)
{
	Options *options;

	global_tool_name = wiom_cli.name;

	options = talloc_zero(context, Options);
	if (options == NULL)
		return NULL;
	options->input_fd  = -1;
	options->output.fd = -1;
	options->actions.filter = ~0UL;

	wiom_cli.private = options;
	return &wiom_cli;
}
