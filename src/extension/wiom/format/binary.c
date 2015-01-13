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

#include <unistd.h>	/* write(2), read(2), */
#include <stdint.h>	/* *int*_t, */
#include <string.h>	/* strcmp(3), */
#include <errno.h>	/* errno(3), E*, */
#include <assert.h>	/* assert(3), */
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

/**
 * Typed wrapper for write_data().
 */
static int write_uint32(int fd, uint32_t value)
{
	return write_data(fd, &value, sizeof(uint32_t));
}

/**
 * Typed wrapper for write_data().
 */
static int write_string(int fd, const char *value)
{
	const size_t size = strlen(value) + 1;
	int status;

	status = write_uint32(fd, size);
	if (status < 0)
		return status;

	return write_data(fd, value, size);
}

/**
 * Dump @history events into @fd.
 */
void report_events_binary(int fd, const Event *history)
{
	const char *header = "WioM_03";
	int status;
	size_t length;
	size_t i;

	if (history == NULL)
		return;

	status = write_string(fd, header);
	if (status < 0)
		goto error;

	length = talloc_array_length(history);
	for (i = 0; i < length; i++) {
		const Event *event = &history[i];

		status = write_uint32(fd, event->pid);
		if (status < 0)
			goto error;

		status = write_uint32(fd, event->action);
		if (status < 0)
			goto error;

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
				goto error;
			break;

		case MOVE_CREATES:
		case MOVE_OVERRIDES:
			status = write_string(fd, event->load.path);
			if (status < 0)
				goto error;

			status = write_string(fd, event->load.path2);
			if (status < 0)
				goto error;

			break;

		case CLONED:
			status = write_uint32(fd, event->load.new_pid);
			if (status < 0)
				goto error;

			status = write_uint32(fd, event->load.flags);
			if (status < 0)
				goto error;

			break;

		case EXITED:
			status = write_uint32(fd, event->load.status);
			if (status < 0)
				goto error;
			break;
		}
	}

	return;

error:
	note(NULL, ERROR, SYSTEM, "can't write event: %s", strerror(-status));
}

/**
 * Read and copy an integer into *@result from the file referenced by
 * @fd at the current offset.  This function returns -errno if an
 * error occurred, 1 if there's nothing else to read (end of file), 0
 * otherwise.
 */
static int read_uint32(int fd, uint32_t *result)
{
	const ssize_t size = sizeof(uint32_t);
	ssize_t status;

	*result = 0;

	status = read(fd, result, size);
	if (status < 0)
		return -errno;

	/* End of file.  */
	if (status == 0)
		return 1;

	if (status != size)
		return -EIO; /* Not the best error code.  */

	return 0;
}

/**
 * Read and duplicate a string into *@result (attached to @context)
 * from the file referenced by @fd at the current offset.  This
 * function returns -errno if an error occurred, 0 otherwise.
 */
static int read_string(TALLOC_CTX *context, int fd, char **result)
{
	ssize_t status;
	uint32_t size;

	assert(result != NULL);
	*result = NULL;

	status = read_uint32(fd, &size);
	if (status < 0)
		return status;

	*result = talloc_size(context, size);
	if (*result == NULL)
		return -ENOMEM;

	status = read(fd, *result, size);
	if (status < 0)
		return -errno;

	if (status != size)
		return -EIO; /* Not the best error code.  */

	return 0;
}

/**
 * Replay events from @config->options->fd.  This function returns
 * -errno if an error occured, 0 otherwise.
 */
int replay_events_binary(TALLOC_CTX *context, SharedConfig *config)
{
	const int fd = config->options->input_fd;
	char *string;
	int status;
	void *tmp;

	/* Fetched strings will be flushed regularly to avoid out of
	 * memory.  */
	tmp = talloc_new(context);
	if (tmp == NULL) {
		status = -ENOMEM;
		goto error;
	}

	status = read_string(tmp, fd, &string);
	if (status < 0)
		goto error;

	if (strcmp(string, "WioM_03") != 0) {
		note(NULL, ERROR, USER, "unknown input file format");
		return -1;
	}

	while (1) {
		uint32_t pid;
		uint32_t action;

		status = read_uint32(fd, &pid);
		if (status < 0)
			goto error;

		/* End of file.  */
		if (status == 1)
			return 0;

		status = read_uint32(fd, &action);
		if (status < 0)
			goto error;

		/* Flush strings.  */
		TALLOC_FREE(tmp);
		tmp = talloc_new(context);
		if (tmp == NULL) {
			status = -ENOMEM;
			goto error;
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
			status = read_string(tmp, fd, &string);
			if (status < 0)
				goto error;

			status = record_event(config, pid, action, string);
			if (status < 0)
				goto error;

			break;

		case MOVE_CREATES:
		case MOVE_OVERRIDES: {
			char *string2;

			status = read_string(tmp, fd, &string);
			if (status < 0)
				goto error;

			status = read_string(tmp, fd, &string2);
			if (status < 0)
				goto error;

			status = record_event(config, pid, action, string, string2);
			if (status < 0)
				goto error;

			break;
		}

		case CLONED: {
			uint32_t new_pid;
			uint32_t flags;

			status = read_uint32(fd, &new_pid);
			if (status < 0)
				goto error;

			status = read_uint32(fd, &flags);
			if (status < 0)
				goto error;

			status = record_event(config, pid, action, new_pid, flags);
			if (status < 0)
				goto error;

			break;
		}

		case EXITED: {
			uint32_t status2;

			status = read_uint32(fd, &status2);
			if (status < 0)
				goto error;

			status = record_event(config, pid, action, status2);
			if (status < 0)
				goto error;

			break;
		}

		default:
			assert(0);
			break;
		}
	};

error:
	note(NULL, ERROR, SYSTEM, "can't read event: %s", strerror(-status));
	return status;
}
