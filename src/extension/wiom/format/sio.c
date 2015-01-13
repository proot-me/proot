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

#include <extension/wiom/wiom.h>

/* Make uthash use talloc.  */
#undef  uthash_malloc
#undef  uthash_free
#define uthash_malloc(size) talloc_size(context, size)
#define uthash_free(pointer, size) TALLOC_FREE(pointer)

typedef enum {
	UNSEEN = 0,
	CREATED,
	DELETED,
	MODIFIED,
	USED,
} State;

typedef struct
{
	UT_hash_handle hh;
	char *path;
	State state;
} HashedPathState;

HashedPathState *fs_state = NULL;

/**
 * Return state of the given @path.
 */
static State get_state(const char *path)
{
	HashedPathState *entry;

	HASH_FIND_STR(fs_state, path, entry);
	if (entry == NULL)
		return UNSEEN;

	return entry->state;
}

/**
 * Set @path's state to @state.
 */
static void set_state(TALLOC_CTX *context, const char *path, State state)
{
	HashedPathState *entry;

	HASH_FIND_STR(fs_state, path, entry);
	if (entry == NULL) {
		entry = talloc(context, HashedPathState);
		if (entry == NULL)
			return;

		entry->path = talloc_strdup(entry, path);
		if (entry->path == NULL) {
			TALLOC_FREE(entry);
			return;
		}

		HASH_ADD_KEYPTR(hh, fs_state, entry->path,
				talloc_get_size(entry->path) - 1, entry);
	}

	entry->state = state;
}

/**
 * A "creates" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_creates(TALLOC_CTX *context, const char *path)
{
	State state = get_state(path);

	switch (state) {
	case UNSEEN:
	case USED:
	case MODIFIED:
		/* CREATED eclipses USED and MODIFIED.  */
		set_state(context, path, CREATED);
		break;

	case DELETED:
		/* That was a temporary path.  */
		set_state(context, path, UNSEEN);
		break;

	case CREATED:
		/* Inconsistency detected.  */
		assert(0);
	}
}

/**
 * A "deletes" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_deletes(TALLOC_CTX *context, const char *path)
{
	State state = get_state(path);

	switch (state) {
	case UNSEEN:
		set_state(context, path, DELETED);
		break;

	case CREATED:
		/* This path was deleted then created,
		 * this is equivalent to modify this
		 * path.  */
		set_state(context, path, MODIFIED);
		break;

	case USED:
	case MODIFIED:
	case DELETED:
		/* Inconsistency detected.  */
		assert(0);
	}
}

/**
 * A "modifies" action happens to @path, change its state with respect
 * to its current state.
 */
static void handle_action_modifies(TALLOC_CTX *context, const char *path)
{
	State state = get_state(path);

	switch (state) {
	case UNSEEN:
	case USED:
		/* MODIFIED eclipses USED.  */
		set_state(context, path, MODIFIED);
		break;

	case MODIFIED:
	case DELETED:
		/* MODIFIED and DELETED eclipse MODIFIED.  */
		break;

	case CREATED:
		/* Inconsistency detected.  */
		assert(0);
		break;
	}

}

/**
 * A "uses" action happens to @path, change its state with respect to
 * its current state.
 */
static void handle_action_uses(TALLOC_CTX *context, const char *path)
{
	State state = get_state(path);

	switch (state) {
	case UNSEEN:
	case USED:
		set_state(context, path, USED);
		break;

	case MODIFIED:
	case CREATED:
	case DELETED:
		/* MODIFIED, CREATED and DELETED eclipse USED.  */
		break;
	}
}

/**
 * Report all events that were stored in @config->history.
 */
void report_events_sio(int fd, const Event *history)
{
	HashedPathState *item;
	void *context;
	size_t length;
	size_t i;

	if (history == NULL)
		return;

	context = talloc_new(NULL);
	if (context == NULL)
		return;

	/* Parse events backward, like for live range analysis.  */
	length = talloc_array_length(history);
	for (i = length - 1; i > 0; i--) {
		const Event *event = &history[i];

		switch (event->action) {
		case CREATES:
			handle_action_creates(context, event->payload.path);
			break;

		case DELETES:
			handle_action_deletes(context, event->payload.path);
			break;

		case SETS_CONTENT_OF:
			handle_action_modifies(context, event->payload.path);
			break;

		case GETS_METADATA_OF:
		case SETS_METADATA_OF:
		case GETS_CONTENT_OF:
		case TRAVERSES:
		case EXECUTES:
			handle_action_uses(context, event->payload.path);
			break;

		case MOVE_CREATES:
			handle_action_deletes(context, event->payload.path);
			handle_action_creates(context, event->payload.path2);
			break;

		case MOVE_OVERRIDES:
			handle_action_deletes(context, event->payload.path);
			handle_action_modifies(context, event->payload.path2);
			break;

		case CLONED:
		case EXITED:
			/* Not used.  */
			break;
		}
	}

	for(item = fs_state; item != NULL; item = item->hh.next) {
		switch (item->state) {
#define CASE(a) case a:							\
			fprintf(stderr, "%s: %s\n", item->path, #a);	\
			break;						\

		CASE(CREATED)
		CASE(DELETED)
		CASE(MODIFIED)
		CASE(USED)
#undef CASE
		default:
			break;
		}
	}

	TALLOC_FREE(context);
	return;
}
