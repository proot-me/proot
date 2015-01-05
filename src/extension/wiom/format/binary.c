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

#include <unistd.h>	/* write(2), */
#include <stdint.h>	/* *int*_t, */
#include <talloc.h>	/* talloc(3), */

#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "cli/note.h"

static int write_string(int fd, const char *value)
{
	const ssize_t size = strlen(value) + 1;
	ssize_t status;

	status = write(fd, value, size);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't write event");
		return status;
	}

	if (status != size)
		note(NULL, ERROR, SYSTEM, "wrote event partially");

	return status;
}

static int write_uint32(int fd, uint32_t value)
{
	const ssize_t size = sizeof(uint32_t);
	ssize_t status;

	status = write(fd, &value, size);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't write event");
		return status;
	}

	if (status != size)
		note(NULL, ERROR, SYSTEM, "wrote event partially");

	return status;

}

void report_events_binary(int fd, const Event *history)
{
	const char *header = "WioM_01";
	int status;
	size_t length;
	size_t i;

	if (history == NULL)
		return;

	status = write_string(fd, header);
	if (status < 0)
		return;

	length = talloc_array_length(history);
	for (i = 0; i < length; i++) {
		const Event *event = &history[i];

		status = write_uint32(fd, event->pid);
		if (status < 0)
			return;

		status = write_uint32(fd, event->action);
		if (status < 0)
			return;

		switch (event->action) {
		case TRAVERSES:
		case CREATES:
		case DELETES:
		case GETS_METADATA_OF:
		case SETS_METADATA_OF:
		case GETS_CONTENT_OF:
		case SETS_CONTENT_OF:
		case EXECUTES:
			status = write_string(fd, event->load.path);
			if (status < 0)
				return;
			break;

		case MOVES:
			status = write_string(fd, event->load.path);
			if (status < 0)
				return;

			status = write_string(fd, event->load.path2);
			if (status < 0)
				return;

			break;

		case IS_CLONED:
			status = write_uint32(fd, event->load.thread);
			if (status < 0)
				return;

			status = write_uint32(fd, event->load.pid);
			if (status < 0)
				return;

			break;

		case HAS_EXITED:
			status = write_uint32(fd, event->load.status);
			if (status < 0)
				return;
			break;
		}
	}
}
