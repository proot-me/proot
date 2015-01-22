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

#include <assert.h>	/* assert(3), */
#include <talloc.h>	/* talloc(3), */
#include <uthash.h>	/* ut*, UT*, HASH*, */
#include <stdio.h>	/* fprintf(3), */

#include <extension/wiom/wiom.h>
#include "cli/note.h"

typedef enum {
	STATE_UNSEEN = 0,
	STATE_CREATED,
	STATE_DELETED,
	STATE_MODIFIED,
	STATE_USED,
} State;

typedef struct
{
	UT_hash_handle hh;
	char *path;
	State state;
} HashedPathState;

typedef struct {
	HashedPathState *entries;
} FileSystemState;

/**
 * Return state of the given @path.
 */
static State get_state(FileSystemState *fs_state, const char *path)
{
	HashedPathState *entry;

	HASH_FIND_STR(fs_state->entries, path, entry);
	if (entry == NULL)
		return STATE_UNSEEN;

	return entry->state;
}

/**
 * Free the memory internally used by uthash.  This is a Talloc
 * destructor.
 */
static void remove_from_hash(HashedPathState *entry)
{
	FileSystemState *fs_state = talloc_get_type_abort(talloc_parent(entry), FileSystemState);
	HASH_DEL(fs_state->entries, entry);
}

/**
 * Set @path's state to @state.
 */
static void set_state(FileSystemState *fs_state, const char *path, State state)
{
	HashedPathState *entry;

	HASH_FIND_STR(fs_state->entries, path, entry);
	if (entry == NULL) {
		entry = talloc_zero(fs_state, HashedPathState);
		if (entry == NULL)
			return;

		entry->path = talloc_strdup(entry, path);
		if (entry->path == NULL) {
			TALLOC_FREE(entry);
			return;
		}

		HASH_ADD_KEYPTR(hh, fs_state->entries, entry->path,
				talloc_get_size(entry->path) - 1, entry);

		talloc_set_destructor(entry, remove_from_hash);
	}

	entry->state = state;
}

/**
 * A "creates" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_creates(FileSystemState *fs_state, const char *path)
{
	State state = get_state(fs_state, path);

	switch (state) {
	case STATE_UNSEEN:
	case STATE_USED:
	case STATE_MODIFIED:
		/* STATE_CREATED eclipses STATE_USED and STATE_MODIFIED.  */
		set_state(fs_state, path, STATE_CREATED);
		break;

	case STATE_DELETED:
		/* That was a temporary path.  */
		set_state(fs_state, path, STATE_UNSEEN);
		break;

	case STATE_CREATED:
		/* Inconsistency detected.  */
		assert(0);
	}
}

/**
 * A "deletes" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_deletes(FileSystemState *fs_state, const char *path)
{
	State state = get_state(fs_state, path);

	switch (state) {
	case STATE_UNSEEN:
		set_state(fs_state, path, STATE_DELETED);
		break;

	case STATE_CREATED:
		/* This path was deleted then created,
		 * this is equivalent to modify this
		 * path.  */
		set_state(fs_state, path, STATE_MODIFIED);
		break;

	case STATE_USED:
	case STATE_MODIFIED:
	case STATE_DELETED:
		/* Inconsistency detected.  */
		assert(0);
	}
}

/**
 * A "modifies" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_modifies(FileSystemState *fs_state, const char *path)
{
	State state = get_state(fs_state, path);

	switch (state) {
	case STATE_UNSEEN:
	case STATE_USED:
		/* STATE_MODIFIED eclipses STATE_USED.  */
		set_state(fs_state, path, STATE_MODIFIED);
		break;

	case STATE_MODIFIED:
	case STATE_DELETED:
		/* STATE_MODIFIED and STATE_DELETED eclipse STATE_MODIFIED.  */
		break;

	case STATE_CREATED:
		/* Inconsistency detected.  */
		assert(0);
		break;
	}

}

/**
 * A "uses" action happens to @path, change its state with respect to
 * its current state.
 */
static void handle_action_uses(FileSystemState *fs_state, const char *path)
{
	State state = get_state(fs_state, path);

	switch (state) {
	case STATE_UNSEEN:
	case STATE_USED:
		set_state(fs_state, path, STATE_USED);
		break;

	case STATE_MODIFIED:
	case STATE_CREATED:
	case STATE_DELETED:
		/* STATE_MODIFIED, STATE_CREATED and STATE_DELETED eclipse STATE_USED.  */
		break;
	}
}

/**
 * Report all events from @config->history into
 * @config->options->output.file.
 */
void report_events_fs_state(const SharedConfig *config)
{
	const HashedString *entry;
	FileSystemState *fs_state;
	HashedPathState *item;
	const char **strings;
	ssize_t i, j;

	if (config->history == NULL)
		return;

	fs_state = talloc_zero(config, FileSystemState);
	if (fs_state == NULL) {
		note(NULL, ERROR, SYSTEM, "can't allocate memory");
		return;
	}

	strings = talloc_array(config, const char *, config->nb_strings);
	if (strings == NULL) {
		note(NULL, ERROR, SYSTEM, "can't allocate memory");
		return;
	}

	for (entry = config->strings, i = 0; entry != NULL; entry = entry->hh.next, i++) {
		assert(entry->index == (uint32_t) i);
		strings[i] = entry->string;
	}

	/* Parse events backward, like for live range analysis.  */
	for (i = talloc_array_length(config->history) - 1; i >= 0; i--) {
		for (j = config->history[i].nb_events - 1; j >= 0; j--) {
			const Event *event = &config->history[i].events[j];

			switch (event->action) {
			case CREATES:
				handle_action_creates(fs_state, strings[event->payload.path]);
				break;

			case DELETES:
				handle_action_deletes(fs_state, strings[event->payload.path]);
				break;

			case SETS_CONTENT_OF:
				handle_action_modifies(fs_state, strings[event->payload.path]);
				break;

			case GETS_METADATA_OF:
			case SETS_METADATA_OF:
			case GETS_CONTENT_OF:
			case TRAVERSES:
			case EXECUTES:
				handle_action_uses(fs_state, strings[event->payload.path]);
				break;

			case MOVE_CREATES:
				handle_action_deletes(fs_state, strings[event->payload.path]);
				handle_action_creates(fs_state, strings[event->payload.path2]);
				break;

			case MOVE_OVERRIDES:
				handle_action_deletes(fs_state, strings[event->payload.path]);
				handle_action_modifies(fs_state, strings[event->payload.path2]);
				break;

			case CLONED:
			case EXITED:
				/* Not used.  */
				break;
			}
		}
	}

	for(item = fs_state->entries; item != NULL; item = item->hh.next) {
		int status;

		switch (item->state) {
#define CASE(a) case STATE_ ##a:					\
			status = fprintf(config->options->output.file,	\
					"%s: %s\n", item->path, #a);	\
			break;						\

		CASE(CREATED)
		CASE(DELETED)
		CASE(MODIFIED)
		CASE(USED)
#undef CASE
		default:
			status = 0;
			break;
		}

		if (status < 0) {
			note(NULL, ERROR, SYSTEM, "can't write event");
			break;
		}
	}

	return;
}
