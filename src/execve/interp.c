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

#include <unistd.h>        /* pread(2), close(2), */
#include <linux/limits.h>  /* PATH_MAX, ARG_MAX, */
#include <string.h>        /* strcpy(3), */
#include <errno.h>         /* -E*, */

#include "execve/interp.h"
#include "execve/elf.h"
#include "compat.h"

/**
 * Extract the ELF interpreter of @host_path in @user_path. This function
 * returns -errno if an error occured, 1 if a ELF interpreter was
 * found and extracted, otherwise 0.
 */
int extract_elf_interp(const Tracee *tracee, const char *host_path,
		       char user_path[PATH_MAX], char argument[ARG_MAX])
{
	ElfHeader elf_header;
	ProgramHeader program_header;

	size_t extra_size;
	int status;
	int fd;

	uint64_t segment_offset;
	uint64_t segment_size;

	user_path[0] = '\0';
	argument[0] = '\0';

	fd = open_elf(host_path, &elf_header);
	if (fd < 0)
		return fd;

	status = find_program_header(tracee, fd, &elf_header, &program_header,
				PT_INTERP, (uint64_t) -1);
	if (status <= 0)
		goto end;

	segment_offset = PROGRAM_FIELD(elf_header, program_header, offset);
	segment_size   = PROGRAM_FIELD(elf_header, program_header, filesz);

	/* If we are executing a host binary under a QEMUlated
	 * environment, we have to access its ELF interpreter through
	 * the "host-rootfs" binding.  Technically it means the host
	 * ELF interpreter "/lib/ld-linux.so.2" is accessed as
	 * "${HOST_ROOTFS}/lib/ld-linux.so.2" to avoid conflict with
	 * the guest "/lib/ld-linux.so.2".  */
	if (tracee->qemu != NULL) {
		strcpy(user_path, HOST_ROOTFS);
		extra_size = strlen(HOST_ROOTFS);
	}
	else
		extra_size = 0;

	if (segment_size + extra_size >= PATH_MAX) {
		status = -EACCES;
		goto end;
	}

	status = pread(fd, user_path + extra_size, segment_size, segment_offset);
	if (status < 0)
		goto end;
	if ((size_t) status != segment_size) { /* Unexpected size.  */
		status = -EACCES;
		goto end;
	}
	user_path[segment_size + extra_size] = '\0';

end:
	close(fd);

	/* Delayed error handling */
	if (status < 0)
		return status;

	/* Is there an INTERP entry? */
	if (user_path[0] == '\0')
		return 0;
	else
		return 1;
}
