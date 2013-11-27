/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2013 STMicroelectronics
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

#include <sys/types.h>   /* open(2), */
#include <sys/stat.h>    /* open(2), */
#include <fcntl.h>       /* open(2), */
#include <unistd.h>      /* read(2), readlink(2), close(2), */
#include <errno.h>       /* errno, EACCES, */
#include <assert.h>      /* assert(3), */
#include <linux/limits.h> /* PATH_MAX, */
#include <string.h>      /* strlen(3), strcmp(3), */
#include <talloc.h>      /* talloc(3), */
#include <archive.h>     /* archive_*(3), */
#include <archive_entry.h> /* archive_entry*(3), */

#include "extension/care/archive.h"
#include "tracee/tracee.h"
#include "cli/notice.h"

#define NB_MAX_SUFFIXES 2
typedef struct {
	int (*set_format)(struct archive *);
	int (*add_filter)(struct archive *);
	int hardlink_resolver_strategy;
	const char *options;
	const char *howto_extract;
	char *const suffixes[NB_MAX_SUFFIXES];
} Format;

static Format supported_formats[] = {
	{
		.suffixes	= { ".cpio", NULL },
		.set_format	= archive_write_set_format_cpio,
		.add_filter	= NULL,
		.options	= NULL,
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_CPIO_POSIX,
		.howto_extract	= "cpio -idmuvF '%s'",
	},
	{
		.suffixes	= { ".cpio.gz", NULL },
		.set_format	= archive_write_set_format_cpio,
		.add_filter	= archive_write_add_filter_gzip,
		.options	= "gzip:compression-level=1",
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_CPIO_POSIX,
		.howto_extract	= "gzip -dc '%s' | cpio -idmuv",
	},
	{
		.suffixes	= { ".cpio.lzo", NULL },
		.set_format	= archive_write_set_format_cpio,
		.add_filter	= archive_write_add_filter_lzop,
		.options	= "lzop:compression-level=1",
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_CPIO_POSIX,
		.howto_extract	= "lzop -dc '%s' | cpio -idmuv",
	},
#if 0
	{
		.suffixes	= { ".tar", NULL },
		.set_format	= archive_write_set_format_gnutar,
		.add_filter	= NULL,
		.options	= NULL,
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_TAR_GNUTAR,
	},
	{
		.suffixes	= { ".tar.gz", ".tgz" },
		.set_format	= archive_write_set_format_gnutar,
		.add_filter	= archive_write_add_filter_gzip,
		.options	= "gzip:compression-level=1",
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_TAR_GNUTAR,
	},
	{
		.suffixes	= { ".tar.lzo", ".tzo" },
		.set_format	= archive_write_set_format_gnutar,
		.add_filter	= archive_write_add_filter_lzop,
		.options	= "lzop:compression-level=1",
		.hardlink_resolver_strategy = ARCHIVE_FORMAT_TAR_GNUTAR,
	},
#endif
	{0},
};

/**
 * Detect the expected format for the given @output.  This function
 * returns NULL on error, otherwise the format descriptor and update
 * @suffix_length with the number of characters that describes the
 * format.
 */
static const Format *detect_format(const Tracee* tracee, const char *output, size_t *suffix_length)
{
	size_t length_output;
	size_t nb_formats;
	size_t i, j;

	assert(output != NULL);

	nb_formats = sizeof(supported_formats) / sizeof(typeof(Format));
	length_output = strlen(output);

	for (i = 0; i < nb_formats; i++) {
		for (j = 0; j < NB_MAX_SUFFIXES; j++) {
			size_t length_suffix;
			const char *suffix;

			suffix = supported_formats[i].suffixes[j];
			if (suffix == NULL)
				continue;

			length_suffix = strlen(suffix);

			if (   length_suffix >= length_output
			    || strcmp(output + length_output - length_suffix, suffix) != 0)
				continue;

			*suffix_length = length_suffix;
			return &supported_formats[i];
		}
	}

	notice(tracee, WARNING, INTERNAL, "unknown format suffix, assuming '%s' format",
		supported_formats[0].suffixes[0]);

	*suffix_length = 0;
	return &supported_formats[0];
}

/**
 * Create a new archive structure (memory allocation attached to
 * @context) for the given @output file.  This function returns NULL
 * on error, otherwise the newly allocated archive structure. See
 * detect_format() for the meaning of @prefix_length.
 */
Archive *new_archive(TALLOC_CTX *context, const Tracee* tracee,
		const char *output, size_t *prefix_length)
{
	const Format *format;
	Archive *archive;
	int status;

	assert(output != NULL);

	format = detect_format(tracee, output, prefix_length);
	assert(format != NULL);

	archive = talloc_zero(context, Archive);
	if (archive == NULL) {
		notice(tracee, ERROR, INTERNAL, "can't allocate archive structure");
		return NULL;
	}

	archive->howto_extract = format->howto_extract;

	archive->handle = archive_write_new();
	if (archive->handle == NULL) {
		notice(tracee, WARNING, INTERNAL, "can't initialize archive structure: %s",
			archive_error_string(archive->handle));
		return NULL;
	}

	assert(format->set_format != NULL);
	status = format->set_format(archive->handle);
	if (status != ARCHIVE_OK) {
		notice(tracee, ERROR, INTERNAL, "can't set archive format: %s",
			archive_error_string(archive->handle));
		return NULL;
	}

	if (format->hardlink_resolver_strategy != 0) {
		archive->hardlink_resolver = archive_entry_linkresolver_new();
		if (archive->hardlink_resolver != NULL)
			archive_entry_linkresolver_set_strategy(archive->hardlink_resolver,
								format->hardlink_resolver_strategy);
	}

	if (format->add_filter != NULL) {
		status = format->add_filter(archive->handle);
		if (status != ARCHIVE_OK) {
			notice(tracee, ERROR, INTERNAL, "can't add archive filter: %s",
				archive_error_string(archive->handle));
			return NULL;
		}
	}

	if (format->options != NULL) {
		status = archive_write_set_options(archive->handle, format->options);
		if (status != ARCHIVE_OK) {
			notice(tracee, ERROR, INTERNAL, "can't set archive options: %s",
				archive_error_string(archive->handle));
			return NULL;
		}
	}

	status = archive_write_open_filename(archive->handle, output);
	if (status != ARCHIVE_OK) {
		notice(tracee, ERROR, INTERNAL, "can't open archive '%s': %s",
			output, archive_error_string(archive->handle));
		return NULL;
	}

	return archive;
}

/**
 * Finalize the given @archive.  This function returns -1 if an error
 * occurred, otherwise 0.
 */
int finalize_archive(Archive *archive)
{
	int status;

	if (archive == NULL || archive->handle == NULL)
		return -1;

	if (archive->hardlink_resolver != NULL)
		archive_entry_linkresolver_free(archive->hardlink_resolver);

	status = archive_write_close(archive->handle);
	if (status != ARCHIVE_OK)
		return -1;

	status = archive_write_free(archive->handle);
	if (status != ARCHIVE_OK)
		return -1;

	return 0;
}

/**
 * Put the content of @path into @archive, with the specified @statl
 * status, at the given @alternate_path (NULL if unchanged).  This
 * function returns -1 if an error occurred, otherwise 0.  Note: this
 * function can be called with @tracee == NULL.
 */
int archive(const Tracee* tracee, Archive *archive,
	const char *path, const char *alternate_path, const struct stat *statl)
{
	struct archive_entry *entry = NULL;
	ssize_t status;
	mode_t type;
	size_t size;
	int fd = -1;

	if (archive == NULL || archive->handle == NULL)
		return -1;

	entry = archive_entry_new();
	if (entry == NULL) {
		notice(tracee, WARNING, INTERNAL, "can't create archive entry for '%s': %s",
			path, archive_error_string(archive->handle));
		status = -1;
		goto end;
	}

	archive_entry_set_pathname(entry, alternate_path ?: path);
	archive_entry_copy_stat(entry, statl);

	if (archive->hardlink_resolver != NULL) {
		struct archive_entry *unused;
		archive_entry_linkify(archive->hardlink_resolver, &entry, &unused);
	}

	/* Get status only once hardlinks were resolved.  */
	size = archive_entry_size(entry);
	type = archive_entry_filetype(entry);

	if (type == AE_IFLNK) {
		char target[PATH_MAX];
		status = readlink(path, target, PATH_MAX);
		if (status >= PATH_MAX) {
			status = -1;
			errno = ENAMETOOLONG;
		}
		if (status < 0) {
			notice(tracee, WARNING, SYSTEM, "can't readlink '%s'", path);
			status = -1;
			goto end;
		}
		target[status] = '\0';

		/* Must be done before archive_write_header().  */
		archive_entry_set_symlink(entry, target);
	}

	status = archive_write_header(archive->handle, entry);
	if (status != ARCHIVE_OK) {
		notice(tracee, WARNING, INTERNAL, "can't write header for '%s': %s",
			path, archive_error_string(archive->handle));
		status = -1;
		goto end;
	}

	/* No content to archive?  */
	if (type != AE_IFREG || size == 0) {
		status = 0;
		goto end;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno != EACCES)
			notice(tracee, WARNING, SYSTEM, "can't open '%s'", path);
		status = -1;
		goto end;
	}

	/* Copy the content from the file into the archive.  */
	do {
		uint8_t buffer[4096];

		status = read(fd, buffer, sizeof(buffer));
		if (status < 0) {
			notice(tracee, WARNING, SYSTEM, "can't read '%s'", path);
			status = -1;
			goto end;
		}

		size = archive_write_data(archive->handle, buffer, status);
		if ((size_t) status != size) {
			notice(tracee, WARNING, INTERNAL, "can't archive '%s' content: %s",
				path, archive_error_string(archive->handle));
			status = -1;
			goto end;
		}
	} while (status > 0);
	status = 0;

end:
	if (fd >= 0)
		(void) close(fd);

	if (entry != NULL)
		archive_entry_free(entry);

	return status;
}
