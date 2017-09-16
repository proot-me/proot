/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#include <sys/types.h>  /* open(2), fstat(2), lseek(2), */
#include <sys/stat.h>   /* open(2), fstat(2), */
#include <sys/stat.h>   /* open(2), */
#include <fcntl.h>      /* open(2), */
#include <stdint.h>     /* *int*_t, *INT*_MAX, */
#include <unistd.h>     /* fstat(2), read(2), lseek(2), */
#include <sys/mman.h>   /* mmap(2), MAP_*, */
#include <stdbool.h>    /* bool, true, false, */
#include <assert.h>     /* assert(3), */
#include <errno.h>      /* errno(3), */
#include <string.h>     /* strerror(3), */
#include <inttypes.h>   /* PRI*, */
#include <endian.h>     /* be64toh(3), */
#include <archive.h>    /* archive_*(3), */
#include <archive_entry.h> /* archive_entry*(3), */

#include "extension/care/extract.h"
#include "cli/note.h"

/**
 * Extract the given @archive into the current working directory.
 * This function returns -1 if an error occured, otherwise 0.
 */
static int extract_archive(struct archive *archive)
{
	struct archive_entry *entry;
	int result = 0;
	int status;

	int flags = ARCHIVE_EXTRACT_PERM
		  | ARCHIVE_EXTRACT_TIME
		  | ARCHIVE_EXTRACT_ACL
		  | ARCHIVE_EXTRACT_FFLAGS
		  | ARCHIVE_EXTRACT_XATTR;

	/* Avoid spurious warnings.  One should test for the CAP_CHOWN
	 * capability instead but libarchive only does this test: */
	if (geteuid() == 0)
		flags |= ARCHIVE_EXTRACT_OWNER;

	while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
		status = archive_read_extract(archive, entry, flags);
		switch (status) {
		case ARCHIVE_OK:
			note(NULL, INFO, USER, "extracted: %s", archive_entry_pathname(entry));
			break;

		default:
			result = -1;
			note(NULL, ERROR, INTERNAL, "%s: %s",
				archive_error_string(archive),
				strerror(archive_errno(archive)));
			break;
		}
	}

	return result;
}

/* Data used by archive_[open/read/close] callbacks.  */
typedef struct
{
	uint8_t buffer[4096];
	const char *path;
	size_t size;
	int fd;
} CallbackData;

/**
 * This callback is invoked by archive_open().  It returns ARCHIVE_OK
 * if the underlying file or data source is successfully opened.  If
 * the open fails, it calls archive_set_error() to register an error
 * code and message and returns ARCHIVE_FATAL.
 *
 *  -- man 3 archive_read_open.
 */
static int open_callback(struct archive *archive, void *data_)
{
	CallbackData *data = talloc_get_type_abort(data_, CallbackData);
	AutoExtractInfo info;
	struct stat statf;
	off_t offset;
	int status;

	/* Note: data->fd will be closed by close_callback().  */
	data->fd = open(data->path, O_RDONLY);
	if (data->fd < 0) {
		archive_set_error(archive, errno, "can't open archive");
		return ARCHIVE_FATAL;
	}

	status = fstat(data->fd, &statf);
	if (status < 0) {
		archive_set_error(archive, errno, "can't stat archive");
		return ARCHIVE_FATAL;
	}

	/* Assume it's a regular archive if it physically can't be a
	 * self-extracting one.  */
	if (statf.st_size < (off_t) sizeof(AutoExtractInfo))
		return ARCHIVE_OK;

	offset = lseek(data->fd, statf.st_size - sizeof(AutoExtractInfo), SEEK_SET);
	if (offset == (off_t) -1) {
		archive_set_error(archive, errno, "can't seek in archive");
		return ARCHIVE_FATAL;
	}

	status = read(data->fd, &info, sizeof(AutoExtractInfo));
	if (status < 0) {
		archive_set_error(archive, errno, "can't read archive");
		return ARCHIVE_FATAL;
	}

	if (   status == sizeof(AutoExtractInfo)
	    && strcmp(info.signature, AUTOEXTRACT_SIGNATURE) == 0) {
		/* This is a self-extracting archive, retrive it's
		 * offset and size.  */

		data->size = be64toh(info.size);
		offset = statf.st_size - data->size - sizeof(AutoExtractInfo);

		note(NULL, INFO, USER,
			"archive found: offset = %" PRIu64 ", size = %" PRIu64 "",
			(uint64_t) offset, data->size);
	}
	else {
		/* This is not a self-extracting archive, assume it's
		 * a regular one...  */
		offset = 0;

		/* ... unless a self-extracting archive really was
		 * expected.  */
		if (strcmp(data->path, "/proc/self/exe") == 0)
			return ARCHIVE_FATAL;
	}

	offset = lseek(data->fd, offset, SEEK_SET);
	if (offset == (off_t) -1) {
		archive_set_error(archive, errno, "can't seek in archive");
		return ARCHIVE_FATAL;
	}

	return ARCHIVE_OK;
}

/**
 * This callback is invoked whenever the library requires raw bytes
 * from the archive.  The read callback reads data into a buffer, set
 * the @buffer argument to point to the available data, and return a
 * count of the number of bytes available.  The library will invoke
 * the read callback again only after it has consumed this data.  The
 * library imposes no constraints on the size of the data blocks
 * returned.  On end-of-file, the read callback returns zero.  On
 * error, the read callback should invoke archive_set_error() to
 * register an error code and message and returns -1.
 *
 *  -- man 3 archive_read_open.
 */
static ssize_t read_callback(struct archive *archive, void *data_, const void **buffer)
{
	CallbackData *data = talloc_get_type_abort(data_, CallbackData);
	ssize_t size;

	size = read(data->fd, data->buffer, sizeof(data->buffer));
	if (size < 0) {
		archive_set_error(archive, errno, "can't read archive");
		return -1;
	}

	*buffer = data->buffer;
	return size;
}

/**
 * This callback is invoked by archive_close() when the archive
 * processing is complete.  The callback returns ARCHIVE_OK on
 * success.  On failure, the callback invokes archive_set_error() to
 * register an error code and message and returns ARCHIVE_FATAL.
 *
 * -- man 3 archive_read_open
 */
static int close_callback(struct archive *archive, void *data_)
{
	CallbackData *data = talloc_get_type_abort(data_, CallbackData);
	int status;

	status = close(data->fd);
	if (status < 0) {
		archive_set_error(archive, errno, "can't close archive");
		return ARCHIVE_WARN;
	}

	return ARCHIVE_OK;
}

/**
 * Extract the archive stored at the given @path.  This function
 * returns -1 if an error occurred, otherwise 0.
 */
int extract_archive_from_file(const char *path)
{
	struct archive *archive = NULL;
	CallbackData *data = NULL;
	int status2;
	int status;

	archive = archive_read_new();
	if (archive == NULL) {
		note(NULL, ERROR, INTERNAL, "can't initialize archive structure");
		status = -1;
		goto end;
	}

	status = archive_read_support_format_cpio(archive);
	if (status != ARCHIVE_OK) {
		note(NULL, ERROR, INTERNAL, "can't set archive format: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_support_format_gnutar(archive);
	if (status != ARCHIVE_OK) {
		note(NULL, ERROR, INTERNAL, "can't set archive format: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_support_filter_gzip(archive);
	if (status != ARCHIVE_OK) {
		note(NULL, ERROR, INTERNAL, "can't add archive filter: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_support_filter_lzop(archive);
	if (status != ARCHIVE_OK) {
		note(NULL, ERROR, INTERNAL, "can't add archive filter: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	data = talloc_zero(NULL, CallbackData);
	if (data == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate callback data");
		status = -1;
		goto end;

	}

	data->path = talloc_strdup(data, path);
	if (data->path == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate callback data path");
		status = -1;
		goto end;

	}

	status = archive_read_open(archive, data, open_callback, read_callback, close_callback);
	if (status != ARCHIVE_OK) {
		/* Don't complain if no error message were registered,
		 * ie. when testing for a self-extracting archive.  */
		if (archive_error_string(archive) != NULL)
			note(NULL, ERROR, INTERNAL, "can't read archive: %s",
				archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = extract_archive(archive);
end:
	if (archive != NULL) {
		status2 = archive_read_close(archive);
		if (status2 != ARCHIVE_OK) {
			note(NULL, WARNING, INTERNAL, "can't close archive: %s",
				archive_error_string(archive));
		}

		status2 = archive_read_free(archive);
		if (status2 != ARCHIVE_OK) {
			note(NULL, WARNING, INTERNAL, "can't free archive: %s",
				archive_error_string(archive));
		}
	}

	TALLOC_FREE(data);

	return status;
}
