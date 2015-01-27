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

#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#include "tracee/tracee.h"

typedef struct {
	struct archive *handle;
	struct archive_entry_linkresolver *hardlink_resolver;

	/* Information used to create an self-extracting archive.  */
	off_t offset;
	int fd;
} Archive;

extern Archive *new_archive(TALLOC_CTX *context, const Tracee* tracee,
				const char *output, size_t *prefix_length);
extern int finalize_archive(Archive *archive);
extern int archive(const Tracee* tracee, Archive *archive,
		const char *path, const char *alternate_path, const struct stat *statl);

#endif /* ARCHIVE_H */
