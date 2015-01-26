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

#include <sched.h>	/* CLONE_THREAD, */
#include <stdio.h>	/* fdopen(3), fprintf(3), */
#include <assert.h>	/* assert(3), */
#include <talloc.h>	/* talloc(3), */

#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "cli/note.h"

/**
 * Report all events from @config->history into
 * @config->options->output.file.
 */
void report_events_trace(const SharedConfig *config)
{
	const char **strings = NULL;
	const HashedString *entry;
	size_t nb_chunks;
	size_t i;
	int status;

	if (config->history == NULL)
		return;

	strings = talloc_array(config, const char *, config->nb_strings);
	if (strings == NULL) {
		note(NULL, ERROR, SYSTEM, "can't allocate memory");
		return;
	}

	for (entry = config->strings, i = 0; entry != NULL; entry = entry->hh.next, i++) {
		assert(entry->index == i);
		strings[i] = entry->string;
	}

	nb_chunks = talloc_array_length(config->history);
	for (i = 0; i < nb_chunks; i++) {
		size_t chunk_size = config->history[i].usage;
		size_t offset;

		offset = 0;
		while (offset < chunk_size) {
			const Event *event = config->history[i].events + offset;
			offset += sizeof(Event);

			switch (event->action) {
#define CASE(a) case a:							\
				status = fprintf(config->options->output.file, \
						"%d %s %s\n",		\
						event->vpid,		\
						#a,			\
						strings[event->payload[0]]); \
				offset += 1 * sizeof(uint32_t);		\
				break;					\

				CASE(TRAVERSES)
				CASE(CREATES)
				CASE(DELETES)
				CASE(GETS_METADATA_OF)
				CASE(SETS_METADATA_OF)
				CASE(GETS_CONTENT_OF)
				CASE(SETS_CONTENT_OF)
#undef CASE

			case EXECUTES:
				status = fprintf(config->options->output.file,
						"%d EXECUTES %s [%s]\n",
						event->vpid,
						strings[event->payload[0]],
						strings[event->payload[1]]);
				offset += 2 * sizeof(uint32_t);
				break;

			case MOVE_CREATES:
				status = fprintf(config->options->output.file,
						"%d MOVE_CREATES %s to %s\n",
						event->vpid,
						strings[event->payload[0]],
						strings[event->payload[1]]);
				offset += 2 * sizeof(uint32_t);
				break;

			case MOVE_OVERRIDES:
				status = fprintf(config->options->output.file,
						"%d MOVE_OVERRIDES %s to %s\n",
						event->vpid,
						strings[event->payload[0]],
						strings[event->payload[1]]);
				offset += 2 * sizeof(uint32_t);
				break;

			case CLONED:
				status = fprintf(config->options->output.file,
						"%d CLONED (%s) into %d\n",
						event->vpid,
						(event->payload[1] & CLONE_THREAD) != 0
						? "thread" : "process",
						event->payload[0]);
				offset += 2 * sizeof(uint32_t);
				break;

			case EXITED:
				status = fprintf(config->options->output.file,
						"%d EXITED (status = %d)\n",
						event->vpid,
						event->payload[0]);
				offset += 1 * sizeof(uint32_t);
				break;

			default:
				assert(0);
				break;
			}

			if (status < 0) {
				note(NULL, ERROR, SYSTEM, "can't write event");
				break;
			}
		}
	}

	return;
}
