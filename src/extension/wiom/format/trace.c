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
	size_t nb_chunks;
	size_t i, j;
	int status;

	if (config->history == NULL)
		return;

	for (i = 0; i < talloc_array_length(config->history); i++) {
		for (j = 0; j < config->history[i].nb_events; j++) {
			const Event *event = &config->history[i].events[j];
			switch (event->action) {
#define CASE(a) case a:							\
				status = fprintf(config->options->output.file, \
						"%d %s %s\n",		\
						event->pid,		\
						#a,			\
						event->payload.path);	\
				break;					\

				CASE(TRAVERSES)
				CASE(CREATES)
				CASE(DELETES)
				CASE(GETS_METADATA_OF)
				CASE(SETS_METADATA_OF)
				CASE(GETS_CONTENT_OF)
				CASE(SETS_CONTENT_OF)
				CASE(EXECUTES)
#undef CASE

			case MOVE_CREATES:
				status = fprintf(config->options->output.file,
						"%d MOVE_CREATES %s to %s\n",
						event->pid,
						event->payload.path,
						event->payload.path2);
				break;

			case MOVE_OVERRIDES:
				status = fprintf(config->options->output.file,
						"%d MOVE_OVERRIDES %s to %s\n",
						event->pid,
						event->payload.path,
						event->payload.path2);
				break;

			case CLONED:
				status = fprintf(config->options->output.file,
						"%d CLONED (%s) into %d\n",
						event->pid,
						(event->payload.flags & CLONE_THREAD) != 0
						? "thread" : "process",
						event->payload.new_pid);
				break;

			case EXITED:
				status = fprintf(config->options->output.file,
						"%d EXITED (status = %ld)\n",
						event->pid,
						event->payload.status);
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
}
