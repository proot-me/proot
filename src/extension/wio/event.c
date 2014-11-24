/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
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
#include <sched.h>	/* CLONE_THREAD, */

#include "extension/wio/event.h"
#include "arch.h"

/*

Coalescing
==========

- "traverses *path*" hides further same instances

- "deletes *path*" unhides all events on *path*

- "gets metadata of *path*" unhides "sets metadata of *path*" instances,
  but hides further same instances

- "sets metadata of *path*" unhides "gets metadata of *path*" instances,
  but hides further same instances

- "sets content of *path*" event unhides "gets content of *path*"
  instances, but hides further same instances

- "gets content of *path*" event unhides "sets content of *path*"
  instances, but hides further same instances

- "moves *path* to *path2*" unhides all events on *path* and *path2*

*/

/* TODO: static struct XXX *history; */
/* TODO: static struct XXX *coalescing; */

#define PRINT(string) do {						\
		path = va_arg(ap, const char *);			\
		fprintf(stderr, "%d %s %s\n", pid, string, path);	\
	} while (0);							\
	break;

/**
 * Record @event from @pid.
 */
void record_event(pid_t pid, Event event, ...)
{
	const char *path2;
	const char *path;
	va_list ap;

	va_start(ap, event);

	switch (event) {
	case TRAVERSES: 	PRINT("traverses");
	case CREATES:		PRINT("creates");
	case DELETES:		PRINT("deletes");
	case GETS_METADATA_OF:	PRINT("gets metadata of");
	case SETS_METADATA_OF:	PRINT("sets metadata of");
	case GETS_CONTENT_OF:	PRINT("gets content of");
	case SETS_CONTENT_OF:	PRINT("sets content of");
	case EXECUTES:		PRINT("executes");

	case MOVES:
		path = va_arg(ap, const char *);
		path2 = va_arg(ap, const char *);
		fprintf(stderr, "%d moves %s to %s\n", pid, path, path2);
		break;

	case IS_CLONED: {
		pid_t child_pid = va_arg(ap, pid_t);
		word_t flags    = va_arg(ap, word_t);
		fprintf(stderr, "%d is cloned (%s) into %d\n", pid,
			(flags & CLONE_THREAD) != 0 ? "thread": "process", child_pid);
		break;
	}

	default:
		assert(0);
		break;
	}

	va_end(ap);

	return;
}
