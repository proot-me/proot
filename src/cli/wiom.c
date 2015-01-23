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
#include <sys/queue.h> 	/* STAILQ_*, LIST_*, */
#include <stdio.h> 	/* strerror(3), fopen(3), */
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

	if (options->output.file != NULL) {
		note(tracee, WARNING, USER, "\"-o %s\" overrides previous choice", value);
		fclose(options->output.file);
	}

	options->output.file = fopen(value, "w");
	if (options->output.file == NULL) {
		note(tracee, ERROR, SYSTEM, "can't open %s", value);
		return -1;
	}

	return 0;
}

static int handle_option_f(Tracee *tracee, const Cli *cli, const char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);

	if (options->output.format != UNSPECIFIED)
		note(tracee, WARNING, USER, "\"-f %s\" overrides previous choice", value);

	if (strcmp(value, "none") == 0)
		options->output.format = NONE;
	else if (strcmp(value, "dump") == 0)
		options->output.format = DUMP;
	else if (strcmp(value, "trace") == 0)
		options->output.format = TRACE;
	else if (  strcmp(value, "fs_state") == 0
		|| strcmp(value, "fs-state") == 0)
		options->output.format = FS_STATE;
	else {
		note(tracee, ERROR, USER, "format '%s' unknown", value);
		return -1;
	}

	return 0;
}

static int handle_filtered_path(Tracee *tracee, Options *options, const char *path, bool masked)
{
	FilteredPath *item;

	if (options->filtered.paths == NULL) {
		options->filtered.paths = talloc_zero(options, FilteredPaths);
		if (options->filtered.paths == NULL) {
			note(tracee, ERROR, SYSTEM, "not enough memory");
			return -1;
		}

		STAILQ_INIT(options->filtered.paths);
	}

	item = talloc_zero(options->filtered.paths, FilteredPath);
	if (item == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	item->path = talloc_strdup(item, path);
	if (item->path == NULL) {
		note(tracee, ERROR, SYSTEM, "not enough memory");
		return -1;
	}

	item->masked = masked;

	STAILQ_INSERT_TAIL(options->filtered.paths, item, link);

	return 0;
}

static int handle_option_p(Tracee *tracee, const Cli *cli, const char *value)
{
	return handle_filtered_path(tracee, talloc_get_type_abort(cli->private, Options), value, true);
}

static int handle_option_P(Tracee *tracee, const Cli *cli, const char *value)
{
	return handle_filtered_path(tracee, talloc_get_type_abort(cli->private, Options), value, false);
}

static int handle_option_q(Tracee *tracee UNUSED, const Cli *cli, const char *value UNUSED)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	options->filtered.mask_pseudo_files = true;
	return 0;
}

static int handle_filtered_action(const Tracee *tracee, void *private, const char *value, bool masked)
{
	Options *options = talloc_get_type_abort(private, Options);
	size_t i;

	#define ACTION(name) #name,
	static const char *known_actions[] = {
		#include "extension/wiom/actions.list"
		NULL,
	};
	#undef ACTION

	if (strcasecmp(value, "all") == 0) {
		if (masked)
			options->filtered.actions = 0;
		else
			options->filtered.actions = ~0UL;
		return 0;
	}

	for (i = 0; known_actions[i] != NULL; i++) {
		if (strcasecmp(value, known_actions[i]) == 0) {
			if (masked)
				UNSET_FILTERED_ACTION_BIT(options, i);
			else
				SET_FILTERED_ACTION_BIT(options, i);
			return 0;
		}
	}

	note(tracee, WARNING, USER, "unknown action '%s'", value);

	note(tracee, INFO, USER, "Supported actions are:");
	note(tracee, INFO, USER, "\tALL");
	for (i = 0; known_actions[i] != NULL; i++)
		note(tracee, INFO, USER, "\t%s", known_actions[i]);

	return -1;
}

static int handle_option_a(Tracee *tracee, const Cli *cli, const char *value)
{
	return handle_filtered_action(tracee, cli->private, value, true);
}

static int handle_option_A(Tracee *tracee, const Cli *cli, const char *value)
{
	return handle_filtered_action(tracee, cli->private, value, false);
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

static int canonicalize_paths(Tracee *tracee, FilteredPaths *list)
{
	FilteredPath *item;
	char path[PATH_MAX];
	int status;

	if (list == NULL)
		return 0;

	STAILQ_FOREACH(item, list, link) {
		status = realpath2(tracee, path, item->path, false);
		if (status < 0) {
			note(tracee, ERROR, SYSTEM, "can't canonicalize '%s': %s",
				(char *) item->path, strerror(-status));
			return -1;
		}

		TALLOC_FREE(item->path);
		item->path = talloc_strdup(item, path);
		if (item->path == NULL) {
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

	if (options->output.format == UNSPECIFIED)
		options->output.format = FS_STATE;

	if (options->output.file == NULL)
		options->output.file = stdout;

	status = canonicalize_paths(tracee, options->filtered.paths);
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
			"the specified command is discarded because of '-i' option.");

	/* Search for WioM configuration.  */
	LIST_FOREACH(extension, tracee->extensions, link) {
		if (extension->callback == wiom_callback) {
			config = extension->config;
			break;
		}
	}
	assert(config != NULL);

	status = replay_events_dump(config->shared);
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
	options->filtered.actions = ~0UL;

	wiom_cli.private = options;
	return &wiom_cli;
}
