/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
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

#include <fcntl.h>  /* open(2), */
#include <unistd.h> /* read(2), close(2), */
#include <limits.h> /* PATH_MAX, ARG_MAX, */
#include <errno.h>  /* ENAMETOOLONG, */
#include <string.h> /* strcpy(3), */

#include "execve/interp.h"
#include "execve/elf.h"
#include "config.h"

#include "compat.h"

/**
 * Extract the shebang of @t_path in @u_interp and @arg_max. This
 * function returns -errno if an error occured, 1 if a shebang was
 * found and extracted, otherwise 0.
 *
 * Extract from "man 2 execve":
 *
 *     On Linux, the entire string following the interpreter name is
 *     passed as a *single* argument to the interpreter, and this
 *     string can include white space.
 */
int extract_script_interp(struct tracee_info *tracee,
			  const char *t_path,
			  char u_interp[PATH_MAX],
			  char argument[ARG_MAX])
{
	char tmp;

	int status;
	int fd;
	int i;

	argument[0] = '\0';

	/* Inspect the executable.  */
	fd = open(t_path, O_RDONLY);
	if (fd < 0)
		return -errno;

	status = read(fd, u_interp, 2 * sizeof(char));
	if (status < 0) {
		status = -errno;
		goto end;
	}
	if (status < 2 * sizeof(char)) { /* EOF */
		status = 0;
		goto end;
	}

	/* Check if it really is a script text. */
	if (u_interp[0] != '#' || u_interp[1] != '!') {
		status = 0;
		goto end;
	}

	/* Skip leading spaces. */
	do {
		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if (status < sizeof(char)) { /* EOF */
			status = 0;
			goto end;
		}
	} while (tmp == ' ' || tmp == '\t');

	/* Slurp the interpreter path until the first space or end-of-line. */
	for (i = 0; i < PATH_MAX; i++) {
		switch (tmp) {
		case ' ':
		case '\t':
			/* Remove spaces in between the interpreter
			 * and the hypothetical argument. */
			u_interp[i] = '\0';
			break;

		case '\n':
		case '\r':
			/* There is no argument. */
			u_interp[i] = '\0';
			argument[0] = '\0';
			status = 1;
			goto end;

		default:
			/* There is an argument if the previous
			 * character in u_interp[] is '\0'. */
			if (i > 1 && u_interp[i - 1] == '\0')
				goto argument;
			else
				u_interp[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if (status < sizeof(char)) { /* EOF */
			u_interp[i] = '\0';
			argument[0] = '\0';
			status = 1;
			goto end;
		}
	}

	/* The interpreter path is too long. */
	status = -ENAMETOOLONG;
	goto end;

argument:
	/* Slurp the argument until the end-of-line. */
	for (i = 0; i < ARG_MAX; i++) {
		switch (tmp) {
		case '\n':
		case '\r':
			argument[i] = '\0';

			/* Remove trailing spaces. */
			for (i--; i > 0 && (argument[i] == ' ' || argument[i] == '\t'); i--)
				argument[i] = '\0';

			status = 1;
			goto end;

		default:
			argument[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if (status < sizeof(char)) { /* EOF */
			argument[0] = '\0';
			status = 1;
			goto end;
		}
	}

	/* The argument is too long, just ignore it. */
	argument[0] = '\0';
end:
	close(fd);

	/* Did an error occur or isn't a script? */
	if (status <= 0)
		return status;

	return 1;
}

/**
 * Extract the ELF interpreter of @path in @u_interp. This function
 * returns -errno if an error occured, 1 if a ELF interpreter was
 * found and extracted, otherwise 0.
 */
int extract_elf_interp(struct tracee_info *tracee,
		       const char *t_path,
		       char u_interp[PATH_MAX],
		       char argument[ARG_MAX])
{
	union elf_header elf_header;
	union program_header program_header;

	size_t extra_size;
	int status;
	int fd;

	uint64_t segment_offset;
	uint64_t segment_size;

	u_interp[0] = '\0';
	argument[0] = '\0';

	fd = open_elf(t_path, &elf_header);
	if (fd < 0)
		return fd;

	status = find_program_header(fd, &elf_header, &program_header,
				PT_INTERP, (uint64_t) -1);
	if (status < 0) {
		status = -EACCES;
		goto end;
	}
	if (status == 0)
		goto end;

	segment_offset = PROGRAM_FIELD(elf_header, program_header, offset);
	segment_size   = PROGRAM_FIELD(elf_header, program_header, filesz);

	status = (int) lseek(fd, segment_offset, SEEK_SET);
	if (status < 0) {
		status = -EACCES;
		goto end;
	}

	/* If we are executing a host binary under a QEMUlated
	 * environment, we have to access its ELF interpreter through
	 * the "host-rootfs" binding.  Technically it means the host
	 * ELF interpreter "/lib/ld-linux.so.2" is accessed as
	 * "${host_rootfs}/lib/ld-linux.so.2" to avoid conflict with
	 * the guest "/lib/ld-linux.so.2".  */
	if (config.qemu) {
		strcpy(u_interp, config.host_rootfs);
		extra_size = strlen(config.host_rootfs);
	}
	else
		extra_size = 0;

	if (segment_size + extra_size >= PATH_MAX) {
		status = -EACCES;
		goto end;
	}

	status = read(fd, u_interp + extra_size, segment_size);
	if (status < 0) {
		status = -EACCES;
		goto end;
	}
	u_interp[segment_size + extra_size] = '\0';

end:
	close(fd);

	/* Delayed error handling */
	if (status < 0)
		return status;

	/* Is there an INTERP entry? */
	if (u_interp[0] == '\0')
		return 0;
	else
		return 1;
}
