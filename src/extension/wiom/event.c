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
#include <sys/queue.h>	/* STAILQ, */

#include "extension/wiom/event.h"
#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "path/path.h"
#include "cli/note.h"
#include "arch.h"

/**
 * Allocate a new event at the end of @config->history, with given
 * @pid, @action anb @nb_payloads.  This function return NULL if an
 * error occurred, otherwise 0.
 */
static Event *new_event(SharedConfig *config, size_t nb_payloads, uint32_t vpid, Action action)
{
	size_t nb_chunks;
	size_t needed_size;
	size_t free_size;
	Event *event;
	void *tmp;

#define HISTORY_CHUNK config->history[nb_chunks - 1]

	if (config->history != NULL) {
		nb_chunks = talloc_array_length(config->history);
		free_size = HISTORY_CHUNK_SIZE - HISTORY_CHUNK.usage;
	}
	else {
		nb_chunks = 0;
		free_size = 0;
	}

	needed_size = sizeof(Event) + nb_payloads * sizeof(uint32_t);
	if (free_size < needed_size) {
		tmp = talloc_realloc(config, config->history, HistoryChunk, nb_chunks + 1);
		if (tmp == NULL)
			return NULL;

		config->history = tmp;
		nb_chunks++;

		HISTORY_CHUNK.events = talloc_size(config->history, HISTORY_CHUNK_SIZE);
		if (HISTORY_CHUNK.events == NULL)
			return NULL;

		HISTORY_CHUNK.usage = 0;
	}

	event = HISTORY_CHUNK.events + HISTORY_CHUNK.usage;

	HISTORY_CHUNK.usage += needed_size;

	event->vpid   = vpid;
	event->action = action;

	return event;

#undef HISTORY_CHUNK
}

/**
 * Free the memory internally used by uthash.  This is a Talloc
 * destructor.
 */
static int remove_from_hash(HashedString *entry)
{
	SharedConfig *config = talloc_get_type_abort(talloc_parent(entry), SharedConfig);

	HASH_DEL(config->strings, entry);

	return 0;
}

/**
 * Get the index in the @config->strings cache. of the @original
 * string.  This function returns -errno if an error occurred, 0
 * otherwise.
 */
static ssize_t get_string_index(SharedConfig *config, const char *original)
{
	HashedString *entry;

	if (original == NULL)
		original = "NULL";

	HASH_FIND_STR(config->strings, original, entry);
	if (entry != NULL)
		return entry->index;

	entry = talloc(config, HashedString);
	if (entry == NULL)
		return -ENOMEM;

	entry->string = talloc_strdup(entry, original);
	if (entry->string == NULL) {
		TALLOC_FREE(entry);
		return -ENOMEM;
	}

	HASH_ADD_KEYPTR(hh, config->strings, entry->string,
			talloc_get_size(entry->string) - 1, entry);

	talloc_set_destructor(entry, remove_from_hash);

	entry->index = config->nb_strings;
	config->nb_strings++;

	return entry->index;
}

/**
 * Check whether @path is masked with respect to @config->options.
 */
static bool is_path_masked(SharedConfig *config, const char *path)
{
	FilteredPath *current;
	Comparison comparison;
	bool masked = false;

	if (path[0] != '/')
		return config->options->mask_pseudo_files;

	if (config->options->filtered.paths == NULL)
		return false;

	STAILQ_FOREACH(current, config->options->filtered.paths, link) {
		/* Does the current filtered path contain this path?
		 * Note that filtered paths are not sorted on
		 * purpose.  */
		comparison = compare_paths(current->path, path);
		if (   comparison == PATHS_ARE_EQUAL
		    || comparison == PATH1_IS_PREFIX)
			masked = current->masked;
	}

	return masked;
}

/**
 * Record event for given @action performed by @vpid.  This function
 * return -errno if an error occurred, otherwise 0.
 */
int record_event(SharedConfig *config, uint64_t vpid, Action action, ...)
{
	const char *path2;
	const char *path;
	const char *argv;
	ssize_t status;
	Event *event;
	va_list ap;

	va_start(ap, action);

	if (GET_FILTERED_ACTION_BIT(config->options, action) == 0) {
		status = 0;
		goto end;
	}

	if ((vpid & 0x00000000) != 0) {
		note(NULL, ERROR, INTERNAL, "wiom: internal vpid overflow detected");
		return -ERANGE;
	}

	switch (action) {
	case TRAVERSES:
	case CREATES:
	case DELETES:
	case GETS_METADATA_OF:
	case SETS_METADATA_OF:
	case GETS_CONTENT_OF:
	case SETS_CONTENT_OF:
		path = va_arg(ap, const char *);
		if (is_path_masked(config, path))
			break;

		event = new_event(config, 1, vpid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		status = get_string_index(config, path);
		if (status < 0)
			goto end;
		event->payload[0] = status;

		break;

	case EXECUTES:
		path = va_arg(ap, const char *);
		argv = va_arg(ap, const char *);
		if (is_path_masked(config, path))
			break;

		event = new_event(config, 2, vpid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		status = get_string_index(config, path);
		if (status < 0)
			goto end;
		event->payload[0] = status;

		if (config->options->discard_argv)
			argv = NULL;

		status = get_string_index(config, argv);
		if (status < 0)
			goto end;
		event->payload[1] = status;

		break;

	case MOVE_CREATES:
	case MOVE_OVERRIDES:
		path  = va_arg(ap, const char *);
		path2 = va_arg(ap, const char *);
		if (is_path_masked(config, path) && is_path_masked(config, path2))
			break;

		event = new_event(config, 2, vpid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		status = get_string_index(config, path);
		if (status < 0)
			goto end;
		event->payload[0] = status;

		status = get_string_index(config, path2);
		if (status < 0)
			goto end;
		event->payload[1] = status;

		break;

	case CLONED: {
		event = new_event(config, 2, vpid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload[0] = (uint32_t) va_arg(ap, uint64_t);
		event->payload[1] = (uint32_t) va_arg(ap, word_t);

		break;
	}

	case EXITED: {
		event = new_event(config, 1, vpid, action);
		if (event == NULL) {
			status = -ENOMEM;
			goto end;
		}

		event->payload[0] = (uint32_t) va_arg(ap, word_t);

		break;
	}

	default:
		assert(0);
		break;
	}

	status = 0;
end:
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "wiom: can't record event: %s",
			strerror(-status));

	va_end(ap);

	return status;
}

/**
 * Report all events that were stored in @config->history.
 */
int report_events(SharedConfig *config)
{
	switch (config->options->output.format) {
	case NONE:
		break;

	case DUMP:
		report_events_dump(config);
		break;

	case TRACE:
		report_events_trace(config);
		break;

	case FS_STATE:
		report_events_fs_state(config);
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

	return 0;
}
