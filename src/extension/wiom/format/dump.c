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

#include <unistd.h>	/* write(2), read(2), fstat(2), */
#include <sys/types.h>	/* fstat(2), */
#include <sys/stat.h>	/* fstat(2), */
#include <sys/mman.h>	/* mmap(2), */
#include <stdint.h>	/* *int*_t, */
#include <string.h>	/* strcmp(3), */
#include <errno.h>	/* errno(3), E*, */
#include <assert.h>	/* assert(3), */
#include <stdlib.h>	/* getenv(3), */
#include <talloc.h>	/* talloc(3), */

#include "extension/wiom/wiom.h"
#include "extension/wiom/format.h"
#include "extension/wiom/event.h"
#include "cli/note.h"

/**
 * Write @size bytes from @value into the file referenced by @fd at
 * the current offset.  This function returns -errno if an error
 * occurred, 0 otherwise.
 */
static int write_data(int fd, const void *value, ssize_t size)
{
	ssize_t status;

	status = write(fd, value, size);
	if (status < 0)
		return -errno;

	if (status != size)
		return -ENOSPC;

	return 0;
}

static int write_string(int fd, const char *value)
{
	return write_data(fd, value, strlen(value) + 1);
}

/**
 * Report all events from @config->history into
 * @config->options->output.file.
 */
void report_events_dump(const SharedConfig *config)
{
	const HashedString *entry;
	size_t size;
	int status;
	size_t i;
	int fd;

	if (config->history == NULL)
		return;

	fd = fileno(config->options->output.file);
	if (fd < 0) {
		status = -errno;
		goto error;
	}

	/* Header.  */
	status = write_string(fd, "WioM_05");
	if (status < 0)
		goto error;

	/* Number of strings.  */
	size = config->nb_strings;
	write_data(fd, &size, sizeof(size));

	/* Strings.  */
	for (entry = config->strings, i = 0; entry != NULL; entry = entry->hh.next, i++) {
		assert(entry->index == i);
		status = write_string(fd, entry->string);
	}

	/* Events.  */
	for (i = 0; i < talloc_array_length(config->history); i++) {
		status = write_data(fd, config->history[i].events,
				config->history[i].nb_events * sizeof(Event));
		if (status < 0)
			goto error;
	}

	return;

error:
	note(NULL, WARNING, INTERNAL, "can't write event: %s", strerror(-status));
}

/**
 * Return *@cursor and increment this latter by @size.  If *@cursor +
 * @size is greater than @end, then NULL is returned and *@cursor is
 * not incremented.
 */
static const void *consume_data(const void **cursor, const void *end, const size_t size)
{
	const void *result;

	if (*cursor > end - size) {
		note(NULL, WARNING, USER, "unexpected end of mapping");
		return NULL;
	}

	result = *cursor;
	*cursor += size;

	return result;
}

static const char *consume_string(const void **cursor, const void *end)
{
	const char *result;

	result = consume_data(cursor, end, strlen(*cursor) + 1);
	if (result == NULL)
		result = "";

	return result;
}

/**
 * Replay events from @config->options->fd, using mmap() syscall.
 * This function returns -errno if an error occured, 0 otherwise.
 */
int replay_events_dump(SharedConfig *config)
{
	const char **strings = NULL;
	const size_t *nb_strings;
	const char *string;

	void *mapping;
	const void *cursor;
	const void *end;

	struct stat statf;
	int status2;
	int status;
	size_t i;

	status = fstat(config->options->input_fd, &statf);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't stat input file");
		return -1;
	}

	mapping = mmap(NULL, statf.st_size, PROT_READ, MAP_PRIVATE, config->options->input_fd, 0);
	if (mapping == MAP_FAILED) {
		note(NULL, ERROR, SYSTEM, "can't map input file");
		return -1;
	}

	cursor = mapping;
	end = mapping + statf.st_size;

	/* Header.  */
	string = consume_string(&cursor, end);
	if (strcmp(string, "WioM_05") != 0) {
		note(NULL, ERROR, USER, "unknown input file format");
		status = -1;
		goto end;
	}

	/* Number of strings.  */
	nb_strings = consume_data(&cursor, end, sizeof(*nb_strings));
	if (nb_strings == NULL) {
		status = -1;
		goto end;
	}

	/* Strings.  */
	strings = talloc_array(config, const char *, *nb_strings);
	if (strings == NULL) {
		status = -1;
		goto end;
	}

	for (i = 0; i < *nb_strings; i++)
		strings[i] = consume_string(&cursor, end);

	/* Events.  */
	while (cursor < end) {
		const Event *event = consume_data(&cursor, end, sizeof(Event));

		switch (event->action) {
		case TRAVERSES:
		case CREATES:
		case DELETES:
		case GETS_METADATA_OF:
		case SETS_METADATA_OF:
		case GETS_CONTENT_OF:
		case SETS_CONTENT_OF:
			if (event->payload.path > *nb_strings) {
				note(NULL, ERROR, INTERNAL, "unexpected string index");
				status = -1;
				break;
			}

			status = record_event(config, event->vpid, event->action,
					strings[event->payload.path]);
			break;

		case EXECUTES:
		case MOVE_CREATES:
		case MOVE_OVERRIDES:
			if (   event->payload.path > *nb_strings
			    || event->payload.path2 > *nb_strings) {
				note(NULL, ERROR, INTERNAL, "unexpected string index");
				status = -1;
				break;
			}

			status = record_event(config, event->vpid, event->action,
					strings[event->payload.path],
					strings[event->payload.path2]);
			break;

		case CLONED:
			status = record_event(config, event->vpid, event->action,
					(uint64_t) event->payload.new_vpid,
					(word_t) event->payload.flags);
			break;

		case EXITED:
			status = record_event(config, event->vpid, event->action,
					(word_t) event->payload.status);
			break;

		default:
			assert(0);
			break;
		}

		if (status < 0)
			goto end;
	}

	status = 0;
end:
	status2 = munmap(mapping, statf.st_size);
	if (status2 < 0)
		note(NULL, WARNING, SYSTEM, "can't unmap input file");

	TALLOC_FREE(strings);

	return status;
}
