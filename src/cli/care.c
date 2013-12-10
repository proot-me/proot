/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of CARE.
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
#include <strings.h>   /* bzero(3), */
#include <sys/queue.h> /* STAILQ_*, */
#include <stdint.h>    /* INT_MIN, */
#include <unistd.h>    /* getpid(2), close(2), */
#include <stdio.h>     /* P_tmpdir, printf(3), fflush(3), */
#include <unistd.h>    /* getcwd(2), */
#include <errno.h>     /* errno(3), */

#include "cli/cli.h"
#include "cli/notice.h"
#include "path/binding.h"
#include "extension/extension.h"
#include "extension/care/care.h"
#include "attribute.h"

/* These should be included last.  */
#include "build.h"
#include "cli/care.h"

static int handle_option_o(Tracee *tracee UNUSED, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	options->output = value;
	return 0;
}

static int handle_option_c(Tracee *tracee UNUSED, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	Item *item = queue_item(options, &options->concealed_paths, value);
	return (item != NULL ? 0 : -1);
}

static int handle_option_r(Tracee *tracee UNUSED, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	Item *item = queue_item(options, &options->revealed_paths, value);
	return (item != NULL ? 0 : -1);
}

static int handle_option_p(Tracee *tracee UNUSED, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	Item *item = queue_item(options, &options->volatile_paths, value);
	return (item != NULL ? 0 : -1);
}

static int handle_option_e(Tracee *tracee UNUSED, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	Item *item = queue_item(options, &options->volatile_envars, value);
	return (item != NULL ? 0 : -1);
}

static int handle_option_m(Tracee *tracee, const Cli *cli, char *value)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	return parse_integer_option(tracee, &options->max_size, value, "-m");
}

static int handle_option_d(Tracee *tracee UNUSED, const Cli *cli, char *value UNUSED)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	options->ignore_default_config = true;
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

extern char __attribute__((weak)) _binary_manual_start;
extern char __attribute__((weak)) _binary_manual_end;

static int handle_option_h(Tracee *tracee UNUSED, const Cli *cli UNUSED, char *value UNUSED)
{
	size_t size;

	size = &_binary_manual_end - &_binary_manual_start;
	if (size != 0)
		write(1, &_binary_manual_start, size);
	else
		printf("No manual found, please visit http://reproducible.io instead.\n");

	exit_failure = false;
	return -1;
}

/**
 * Delete this temporary directory.  Note: this is a Talloc
 * desctructor.
 */
static int remove_temp(char *path)
{
	char *command;

	/* Sanity checks.  */
	assert(strncmp(P_tmpdir, path, strlen(P_tmpdir)) == 0);
	assert(path[0] == '/');

	command = talloc_asprintf(NULL, "rm -rf %s 2>/dev/null", path);
	if (command != NULL) {
		int status;

		status = system(command);
		if (status != 0)
			notice(NULL, INFO, USER, "can't delete '%s'", path);
	}

	TALLOC_FREE(command);

	return 0;
}

/**
 * Allocate a new binding for the given @tracee that will conceal the
 * content of @path with an empty file or directory.  This function
 * complains about missing @host path only if @must_exist is true.
 */
static Binding *new_concealing_binding(Tracee *tracee, const char *path, bool must_exist)
{
	struct stat statl;
	Binding *binding;
	int status;
	char *temp;

	status = stat(path, &statl);
	if (status < 0) {
		if (must_exist)
			notice(tracee, WARNING, SYSTEM, "can't conceal %s", path);
		return NULL;
	}

	temp = talloc_asprintf(tracee, "%s/care-%d-XXXXXX", P_tmpdir, getpid());
	if (temp == NULL) {
		notice(tracee, WARNING, INTERNAL, "can't conceal %s: not enough memory", path);
		return NULL;
	}

	/* Create the empty file or directory.  */
	if (S_ISDIR(statl.st_mode)) {
		if (mkdtemp(temp) == NULL)
			status = -errno;
	}
	else {
		status = mkstemp(temp);
		if (status >= 0)
			close(status);
	}
	if (status < 0) {
		notice(tracee, WARNING, SYSTEM, "can't conceal %s", path);
		return NULL;
	}

	binding = new_binding(tracee, temp, path, must_exist);
	if (binding == NULL)
		return NULL;

	/* Make sure the temporary directory will be removed at the
	 * end of the execution.  */
	talloc_reparent(tracee, binding, temp);
	talloc_set_destructor(temp, remove_temp);

	return binding;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot/CARE but
 * that are not specifiable on the command line.
 */
static int pre_initialize_bindings(Tracee *tracee, const Cli *cli,
				size_t argc, char *const *argv, size_t cursor)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	char path[PATH_MAX];
	Binding *binding;
	const char *home;
	const char *pwd;
	Item *item;
	size_t i;

	if (cursor >= argc) {
		notice(tracee, ERROR, USER, "no command specified");
		return -1;
	}
	options->command = &argv[cursor];

	home = getenv("HOME");
	pwd  = getenv("PWD");

	/* Set these variables to their default values (ie. when not
	 * set), this simplifies the binding setup to the related
	 * files.  */
	setenv("XAUTHORITY",   talloc_asprintf(tracee->ctx, "%s/.Xauthority", home), 0);
	setenv("ICEAUTHORITY", talloc_asprintf(tracee->ctx, "%s/.ICEauthority", home), 0);

	/* Enable default option first.  */
	if (!options->ignore_default_config) {
		const char *expanded;
		Binding *binding;
		int status;

		/* Bind an empty file/directory over default concealed
		 * paths.  */
		for (i = 0; default_concealed_paths[i] != NULL; i++) {
			expanded = expand_front_variable(tracee->ctx, default_concealed_paths[i]);
			binding = new_concealing_binding(tracee, expanded, false);
			if (binding != NULL)
				VERBOSE(tracee, 0, "concealed path: %s %s",
					default_concealed_paths[i],
					default_concealed_paths[i] != expanded ? expanded : "");
		}

		/* Bind default revealed paths over concealed
		 * paths.  */
		for (i = 0; default_revealed_paths[i] != NULL; i++) {
			expanded = expand_front_variable(tracee->ctx, default_revealed_paths[i]);
			binding = new_binding(tracee, expanded, NULL, false);
			if (binding != NULL)
				VERBOSE(tracee, 0, "revealed path: %s %s",
					default_revealed_paths[i],
					default_revealed_paths[i] != expanded ? expanded : "");
		}

		/* Ensure the initial command is accessible.  */
		status = which(NULL, NULL, path, options->command[0]);
		if (status < 0)
			return -1; /* This failure was already noticed by which().  */

		binding = new_binding(tracee, path, NULL, false);
		if (binding != NULL)
			VERBOSE(tracee, 0, "revealed path: %s", path);

		/* Sanity check.  Note: it is assumed $HOME and $PWD
		 * are canonicalized.  */
		if (home != NULL && pwd != NULL && strcmp(home, pwd) == 0)
			notice(tracee, WARNING, USER,
				"$HOME is implicitely revealed since it is the same as $PWD, "
				"change your current working directory to be sure "
				"your personal data will be not archivable.");

		/* Add the default volatile paths to the list of user
		 * volatile paths.  */
		for (i = 0; default_volatile_paths[i] != NULL; i++) {
			Item *item;

			expanded = expand_front_variable(tracee->ctx, default_volatile_paths[i]);
			item = queue_item(tracee, &options->volatile_paths, expanded);

			/* Remember the non expanded form, later used
			 * by archive_re_execute_sh(). */
			if (item != NULL && expanded != default_volatile_paths[i])
				talloc_set_name_const(item, default_volatile_paths[i]);
		}

		for (i = 0; default_volatile_envars[i] != NULL; i++)
			queue_item(tracee, &options->volatile_envars, default_volatile_envars[i]);

		if (options->max_size == INT_MIN)
			options->max_size = CARE_MAX_SIZE;
	}
	else
		if (options->max_size == INT_MIN)
			options->max_size = -1; /* Unlimited.  */

	VERBOSE(tracee, 1, "max size: %d", options->max_size);

	/* Bind an empty file/directory over user concealed paths.  */
	if (options->concealed_paths != NULL) {
		STAILQ_FOREACH(item, options->concealed_paths, link) {
			binding = new_concealing_binding(tracee, item->load, true);
			if (binding != NULL)
				VERBOSE(tracee, 0, "concealed path: %s", (char *) item->load);
		}
	}

	/* Bind user revealed paths over concealed paths.  */
	if (options->revealed_paths != NULL) {
		STAILQ_FOREACH(item, options->revealed_paths, link) {
			binding = new_binding(tracee, item->load, NULL, true);
			if (binding != NULL)
				VERBOSE(tracee, 0, "revealed path: %s",	(char *) item->load);
		}
	}

	/* Bind volatile paths over concealed paths.  */
	if (options->volatile_paths != NULL) {
		STAILQ_FOREACH(item, options->volatile_paths, link) {
			binding = new_binding(tracee, item->load, NULL, false);
			if (binding != NULL)
				VERBOSE(tracee, 1, "volatile path: %s",	(char *) item->load);
		}
	}

	VERBOSE(tracee, 0, "----------------------------------------------------------------------");

	/* Initialize @tracee->fs->cwd with a path already canonicalized
	 * as required by care.c:handle_initialization().  */
	if (getcwd(path, PATH_MAX) == NULL) {
		notice(tracee, ERROR, SYSTEM, "can't get current working directory");
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

/**
 * Initialize CARE extensions.
 */
static int post_initialize_bindings(Tracee *tracee, const Cli *cli,
				size_t argc UNUSED, char *const *argv UNUSED, size_t cursor)
{
	Options *options = talloc_get_type_abort(cli->private, Options);
	int status;

	status = initialize_extension(tracee, care_callback, (void *) options);
	if (status < 0) {
		notice(tracee, WARNING, INTERNAL, "can't initialize the care extension");
		return -1;
	}

	status = initialize_extension(tracee, kompat_callback, NULL);
	if (status < 0) {
		notice(tracee, WARNING, INTERNAL, "can't initialize the kompat extension");
		return -1;
	}

	return cursor;
}

const Cli *get_care_cli(TALLOC_CTX *context)
{
	Options *options;

	options = talloc_zero(context, Options);
	if (options == NULL)
		return NULL;
	options->max_size = INT_MIN;

	care_cli.private = options;
	return &care_cli;
}
