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
 * Report all events from @config->history into
 * @config->options->output.file.
 */
void report_events_dump(const SharedConfig *config)
{
	const char *header = "WioM_03";
	size_t i, j;
	int status;
	int fd;

	if (config->history == NULL)
		return;

	fd = fileno(config->options->output.file);
	if (fd < 0) {
		status = -errno;
		goto error;
	}

	status = write_string(fd, header);
	if (status < 0)
		goto error;

	for (i = 0; i < talloc_array_length(config->history); i++) {
		for (j = 0; j < config->history[i].nb_events; j++) {
			const Event *event = &config->history[i].events[j];

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
				status = write_string(fd, event->payload.path);
				if (status < 0)
					goto error;
				break;

			case MOVE_CREATES:
			case MOVE_OVERRIDES:
				status = write_string(fd, event->payload.path);
				if (status < 0)
					goto error;

				status = write_string(fd, event->payload.path2);
				if (status < 0)
					goto error;

				break;

			case CLONED:
				status = write_uint32(fd, event->payload.new_pid);
				if (status < 0)
					goto error;

				status = write_uint32(fd, event->payload.flags);
				if (status < 0)
					goto error;

				break;

			case EXITED:
				status = write_uint32(fd, event->payload.status);
				if (status < 0)
					goto error;
				break;
			}
		}
	}

	return;

error:
	note(NULL, WARNING, INTERNAL, "can't write event: %s", strerror(-status));
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
 * Replay events from @config->options->fd, using read() syscalls.
 * This function returns -errno if an error occured, 0 otherwise.
 */
static int replay_events_dump_slow(TALLOC_CTX *context, SharedConfig *config)
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
			uint32_t exit_code;

			status = read_uint32(fd, &exit_code);
			if (status < 0)
				goto error;

			status = record_event(config, pid, action, exit_code);
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
	note(NULL, WARNING, INTERNAL, "can't read event: %s", strerror(-status));
	return status;
}

/**
 * Return *@cursor and increment this latter by @size.  If *@cursor +
 * @size is greater than @end, then NULL is returned and *@cursor is
 * not incremented.
 */
static const void *slurp_data(const void **cursor, const size_t size, const void *end)
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

/**
 * Return the uint32_t value pointed by *@cursor.
 */
static uint32_t slurp_uint32(const void **cursor, const void *end)
{
	const uint32_t *result;

	result = slurp_data(cursor, sizeof(uint32_t), end);
	if (result == NULL)
		return 0;

	return *result;
}

/**
 * Return the string pointed by *@cursor.
 */
static const char *slurp_string(const void **cursor, const void *end)
{
	const char *result;
	uint32_t size;

	size = slurp_uint32(cursor, end);

	result = slurp_data(cursor, size, end);
	if (result == NULL)
		result = "";

	return result;
}

/**
 * Replay events from @config->options->fd, using mmap() syscall.
 * This function returns -errno if an error occured, 0 otherwise.
 */
static int replay_events_dump_fast(SharedConfig *config)
{
	const int fd = config->options->input_fd;
	struct stat statf;
	void *mapping;
	const void *cursor;
	const void *end;
	const char *string;
	int status2;
	int status;

	if (getenv("WIOM_NO_MMAP") != NULL)
		return -1;

	status = fstat(fd, &statf);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't stat input file");
		return -1;
	}

	mapping = mmap(NULL, statf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mapping == MAP_FAILED) {
		note(NULL, ERROR, SYSTEM, "can't map input file");
		status = -1;
		goto end;
	}

	cursor = mapping;
	end = mapping + statf.st_size;

	string = slurp_string(&cursor, end);

	if (strcmp(string, "WioM_03") != 0) {
		note(NULL, ERROR, USER, "unknown input file format");
		status = -1;
		goto end;
	}

	while (cursor < end) {
		uint32_t pid;
		uint32_t action;

		pid    = slurp_uint32(&cursor, end);
		action = slurp_uint32(&cursor, end);

		switch (action) {
		case TRAVERSES:
		case CREATES:
		case DELETES:
		case GETS_METADATA_OF:
		case SETS_METADATA_OF:
		case GETS_CONTENT_OF:
		case SETS_CONTENT_OF:
		case EXECUTES:
			string = slurp_string(&cursor, end);

			status = record_event(config, pid, action, string);
			if (status < 0)
				goto end;
			break;

		case MOVE_CREATES:
		case MOVE_OVERRIDES: {
			char const *string2;

			string  = slurp_string(&cursor, end);
			string2 = slurp_string(&cursor, end);

			status = record_event(config, pid, action, string, string2);
			if (status < 0)
				goto end;

			break;
		}

		case CLONED: {
			uint32_t new_pid;
			uint32_t flags;

			new_pid = slurp_uint32(&cursor, end);
			flags   = slurp_uint32(&cursor, end);

			status = record_event(config, pid, action, new_pid, flags);
			if (status < 0)
				goto end;

			break;
		}

		case EXITED: {
			uint32_t exit_code;

			exit_code = slurp_uint32(&cursor, end);

			status = record_event(config, pid, action, exit_code);
			if (status < 0)
				goto end;

			break;
		}

		default:
			assert(0);
			break;
		}
	};

	status = 0;
end:
	status2 = munmap(mapping, statf.st_size);
	if (status2 < 0)
		note(NULL, WARNING, SYSTEM, "can't unmap input file");

	return status;
}

/**
 * Replay events from @config->options->fd.  This function returns
 * -errno if an error occured, 0 otherwise.
 */
int replay_events_dump(TALLOC_CTX *context, SharedConfig *config)
{
	int status;

	status = replay_events_dump_fast(config);
	if (status < 0) {
		note(NULL, INFO, INTERNAL, "fast replay has failed, using slow replay");
		status = replay_events_dump_slow(context, config);
	}

	return status;
}
