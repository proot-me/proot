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

#include <stdarg.h>	/* va_*, */
#include <stdio.h>	/* fprintf(3), */
#include <assert.h>	/* assert(3), */
#include <stdbool.h>	/* bool, */
#include <errno.h>	/* E*, */
#include <unistd.h>	/* close(2), */
#include <talloc.h>	/* talloc(3), */
#include <uthash.h>	/* UT*, */
#include <sys/queue.h>	/* SIMPLEQ, */

#include "extension/wiom/event.h"
#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "path/path.h"
#include "cli/note.h"
#include "arch.h"

/*

Coalescing (TODO)
==========

- "*pid* traverses *path*" hides further instances (same *pid*, same
  action, and same *path*)

- "*pid* deletes *path*" unhides all events on *path* for all pids

- "*pid* gets metadata of *path*" hides further instances (same *pid*,
  same action, and same *path*), but unhides "sets metadata of *path*"
  for all pids

- "*pid* sets metadata of *path*" hides further instances (same *pid*,
  same action, and same *path*), but unhides "gets metadata of *path*"
  for all pids

- "sets content of *path*" hides further instances (same *pid*, same
  action, and same *path*), but unhides "gets content of *path*" for
  all pids

- "gets content of *path*" hides further instances (same *pid*, same
  action, and same *path*), but unhides "sets content of *path*" for
  all pids

- "moves *path* to *path2*" unhides all events on *path* and *path2*,
  for all pids.

- "*pid* has exited" unhides all events for *pid*.

*/

/**
 * Allocate a new event, with given @pid and @action, at the end of
 * @config->history.  This function return NULL if an error occurred,
 * otherwise 0.
 */
static Event *new_event(SharedConfig *config, pid_t pid, Action action)
{
	size_t index;
	Event *event;
	void *tmp;

	if (config->history == NULL) {
		index = 0;

		config->history = talloc_array(config, Event, 1);
		if (config->history == NULL)
			return NULL;
	}
	else {
		index = talloc_array_length(config->history);

		tmp = talloc_realloc(NULL, config->history, Event, index + 1);
		if (tmp == NULL)
			return NULL;
		config->history = tmp;
	}

	event = &config->history[index];

	event->pid = pid;
	event->action = action;

	return event;
}

/**
 * Return a copy of @original from @config->strings cache.
 */
static const char *get_string_copy(SharedConfig *config, const char *original)
{
	HashedString *entry;

	HASH_FIND_STR(config->strings, original, entry);
	if (entry != NULL)
		return entry->string;

	entry = talloc(config, HashedString);
	if (entry == NULL)
		return NULL;

	entry->string = talloc_strdup(entry, original);
	if (entry->string == NULL) {
		TALLOC_FREE(entry);
		return NULL;
	}

	HASH_ADD_KEYPTR(hh, config->strings, entry->string,
			talloc_get_size(entry->string) - 1, entry);

	return entry->string;
}

/**
 * Check whether @path is masked with respect to @config->options.
 */
static bool is_masked(SharedConfig *config, const char *path)
{
	bool masked = false;
	Comparison comparison;
	Item *item;

	if (config->options->paths.masked == NULL)
		return false;

	SIMPLEQ_FOREACH(item, config->options->paths.masked, link) {
		comparison = compare_paths(item->payload, path);
		if (   comparison == PATHS_ARE_EQUAL
		    || comparison == PATH1_IS_PREFIX) {
			masked = true;
			break;
		}
	}

	if (!masked || config->options->paths.unmasked == NULL)
		return masked;

	SIMPLEQ_FOREACH(item, config->options->paths.unmasked, link) {
		comparison = compare_paths(item->payload, path);
		if (   comparison == PATHS_ARE_EQUAL
		    || comparison == PATH1_IS_PREFIX) {
			masked = false;
			break;
		}
	}

	return masked;
}

/**
 * Record event for given @action performed by @pid.  This function
 * return -errno if an error occurred, otherwise 0.
 */
int record_event(SharedConfig *config, pid_t pid, Action action, ...)
{
	const char *path2;
	const char *path;
	Event *event;
	int status;
	va_list ap;

	va_start(ap, action);

	if (GET_ACTION_BIT(config->options, action) == 0) {
		status = 0;
		goto end;
	}

	switch (action) {
	case TRAVERSES:
	case CREATES:
	case DELETES:
	case GETS_METADATA_OF:
	case SETS_METADATA_OF:
	case GETS_CONTENT_OF:
	case SETS_CONTENT_OF:
	case EXECUTES:
		path = va_arg(ap, const char *);
		if (is_masked(config, path))
			break;

		event = new_event(config, pid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload.path = get_string_copy(config, path);
		if (event->payload.path == NULL) {
			status = -ENOMEM;
			goto end;
		}

		break;

	case MOVE_CREATES:
	case MOVE_OVERRIDES:
		path  = va_arg(ap, const char *);
		path2 = va_arg(ap, const char *);
		if (is_masked(config, path) && is_masked(config, path2))
			break;

		event = new_event(config, pid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload.path = get_string_copy(config, path);
		if (event->payload.path == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload.path2 = get_string_copy(config, path2);
		if (event->payload.path2 == NULL) {
			status = -ENOMEM;
			goto end;
		}

		break;

	case CLONED: {
		event = new_event(config, pid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload.new_pid = va_arg(ap, pid_t);
		event->payload.flags   = va_arg(ap, word_t);

		break;
	}

	case EXITED: {
		event = new_event(config, pid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload.status = va_arg(ap, word_t);

		break;
	}

	default:
		assert(0);
		break;
	}

	status = 0;
end:
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "wiom: can't record event: %s\n",
			strerror(-status));

	va_end(ap);

	return status;
}

/**
 * Report all events that were stored in @config->history.
 */
void report_events(SharedConfig *config)
{
	switch (config->options->output.format) {
	case BINARY:
		report_events_binary(config->options->output.file, config->history);
		break;

	case RAW:
		report_events_raw(config->options->output.file, config->history);
		break;

	case FS_STATE:
		report_events_fs_state(config->options->output.file, config->history);
		break;

	case KCONFIG_FS_USAGE:
	case KCONFIG_PROCESS_TREE:
	case KCONFIG_FS_DEPENDENCIES:
	case GMAKE_FS_DEPENDENCIES:
		note(NULL, ERROR, INTERNAL, "this format is not yet implemented");
		break;

	default:
		assert(0);
	}

	fclose(config->options->output.file);
	config->options->output.file = NULL;
}
