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

#include <sys/types.h>  /* open(2), stat(2), */
#include <sys/stat.h>   /* open(2), stat(2), */
#include <fcntl.h>      /* open(2), */
#include <unistd.h>     /* stat(2), */
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
#include "cli/notice.h"

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
			notice(NULL, INFO, USER, "extracted: %s", archive_entry_pathname(entry));
			break;

		default:
			result = -1;
			notice(NULL, ERROR, INTERNAL, "%s: %s",
				archive_error_string(archive),
				strerror(archive_errno(archive)));
			break;
		}
	}

	return result;
}

/**
 * Extract the archive pointed to by @pointer, with the given @size.
 * This function returns -1 if an error occured, otherwise 0.
 */
static int extract_archive_from_memory(void *pointer, size_t size)
{
	int status2;
	int status;

	struct archive *archive = NULL;

	archive = archive_read_new();
	if (archive == NULL) {
		notice(NULL, ERROR, INTERNAL, "can't initialize archive structure");
		status = -1;
		goto end;
	}

	status = archive_read_support_format_cpio(archive);
	if (status != ARCHIVE_OK) {
		notice(NULL, ERROR, INTERNAL, "can't set archive format: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_support_filter_gzip(archive);
	if (status != ARCHIVE_OK) {
		notice(NULL, ERROR, INTERNAL, "can't add archive filter: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_support_filter_lzop(archive);
	if (status != ARCHIVE_OK) {
		notice(NULL, ERROR, INTERNAL, "can't add archive filter: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = archive_read_open_memory(archive, pointer, size);
	if (status != ARCHIVE_OK) {
		notice(NULL, ERROR, INTERNAL, "can't read archive: %s",
			archive_error_string(archive));
		status = -1;
		goto end;
	}

	status = extract_archive(archive);
end:
	if (archive != NULL) {
		status2 = archive_read_close(archive);
		if (status2 != ARCHIVE_OK) {
			notice(NULL, WARNING, INTERNAL, "can't close archive: %s",
				archive_error_string(archive));
		}

		status2 = archive_read_free(archive);
		if (status2 != ARCHIVE_OK) {
			notice(NULL, WARNING, INTERNAL, "can't free archive: %s",
				archive_error_string(archive));
		}
	}

	return status;
}

/**
 * Extract the archive stored at the given @path.  This function
 * returns -1 if an error occurred, otherwise 0.
 */
int extract_archive_from_file(const char *path)
{
	struct stat statf;
	void *pointer;
	uint64_t size;
	int status2;
	int status;

	void *mmapping = NULL;
	int fd = -1;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		status = -errno;
		notice(NULL, ERROR, SYSTEM, "can't open \"%s\"", path);
		goto end;
	}

	status = fstat(fd, &statf);
	if (status < 0) {
		status = -errno;
		notice(NULL, ERROR, SYSTEM, "can't stat \"%s\"", path);
		goto end;
	}

	mmapping = mmap(NULL, statf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mmapping == MAP_FAILED) {
		status = -errno;
		notice(NULL, ERROR, SYSTEM, "can't mmap \"%s\"", path);
		goto end;
	}

	pointer = mmapping;
	size = statf.st_size;

	/* Check if it's an auto-extractable archive.  */
	if (size > sizeof(AutoExtractInfo)) {
		const AutoExtractInfo *info;

		info = (AutoExtractInfo *) ((uintptr_t) pointer + size - sizeof(AutoExtractInfo));

		if (strcmp(info->signature, AUTOEXTRACT_SIGNATURE) == 0) {
			/* The size was stored in big-endian byte order.  */
			size = be64toh(info->size);
			pointer = (void *)((uintptr_t) info - size);

			notice(NULL, INFO, USER,
				"archive found: offset = %" PRIu64 ", size = %" PRIu64 "",
				(uint64_t)(pointer - mmapping), size);
		}
		/* Don't go further if an auto-extractable archive was
		 * expected.  */
		else if (strcmp(path, "/proc/self/exe") == 0)
			return -1;
	}

	status = extract_archive_from_memory(pointer, size);
end:
	if (fd >= 0) {
		status2 = close(fd);
		if (status2 < 0)
			notice(NULL, WARNING, SYSTEM, "can't close \"%s\"", path);
	}

	if (mmapping != NULL) {
		status2 = munmap(mmapping, statf.st_size);
		if (status2 < 0)
			notice(NULL, WARNING, SYSTEM, "can't munmap \"%s\"", path);
	}

	return status;
}
