/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 * Inspired by: execve(2) from the Linux kernel.
 */

#include <fcntl.h>  /* open(2), */
#include <unistd.h> /* access(2), read(2), close(2), */
#include <string.h> /* strcpy(3), */
#include <limits.h> /* PATH_MAX, ARG_MAX, */
#include <errno.h>  /* ENAMETOOLONG, */
#include <assert.h> /* assert(3), */
#include <stdint.h> /* *int*_t */

#include "interp.h"
#include "path.h"
#include "execve.h"
#include "notice.h"
#include "elf.h"

/**
 * Expand the shebang of @path in *@argv[]. This function returns
 * -errno if an error occured, 1 if a shebang was found and expanded,
 * otherwise 0. @filename is updated with the name of the interpreter,
 * if any.
 *
 * Extract from "man 2 execve":
 *
 *     On Linux, the entire string following the interpreter name is
 *     passed as a *single* argument to the interpreter, and this
 *     string can include white space.
 */
int expand_script_interp(struct child_info *child, const char *path, char filename[PATH_MAX], char **argv[])
{
	char interpreter[PATH_MAX];
	char argument[ARG_MAX];
	char tmp;

	int status;
	int fd;
	int i;

	/* Inspect the executable.  */
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -errno;

	status = read(fd, interpreter, 2 * sizeof(char));
	if (status < 0) {
		status = -errno;
		goto end;
	}
	if (status < 2 * sizeof(char)) {
		status = 0;
		goto end;
	}

	/* Check if it really is a script text. */
	if (interpreter[0] != '#' || interpreter[1] != '!') {
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
		if (status < sizeof(char)) {
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
			interpreter[i] = '\0';
			break;

		case '\n':
		case '\r':
			/* There is no argument. */
			interpreter[i] = '\0';
			status = 1;
			goto end;

		default:
			/* There is an argument if the previous
			 * character in interpreter[] is '\0'. */
			if (i > 1 && interpreter[i - 1] == '\0')
				goto argument;
			else
				interpreter[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if (status < sizeof(char)) {
			status = 0;
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

			status = 2;
			goto end;

		default:
			argument[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status != sizeof(char)) {
			status = 0;
			goto end;
		}
	}

	/* The argument is too long, just ignore it. */
	argument[0] = '\0';
	status = 1;

end:
	close(fd);

	/* Did an error occur? */
	if (status <= 0)
		return status;

	/* Special invocation to check if a script interpreter use
	 * a[nother] script interpreter, this is a sanity check made
	 * by execve.c:expand_interp(). */
	if (filename == NULL)
		return -EPERM;

	VERBOSE(3, "expand shebang: %s -> %s %s %s",
		(*argv)[0], interpreter, argument, filename);

	switch (status) {
	case 1:
		status = substitute_argv0(argv, 2, interpreter, filename);
		break;

	case 2:
		status = substitute_argv0(argv, 3, interpreter, argument, filename);
		break;

	default:
		assert(0);
		break;
	}
	if (status < 0)
		return status;

	strcpy(filename, interpreter);
	return 1;
}

/**
 * Expand the ELF interpreter of @path in *@argv[]. This function
 * returns -errno if an error occured, 1 if a ELF interpreter was
 * found and expanded, otherwise 0.  @filename is updated with the
 * name of the interpreter, if any.
 */
int expand_elf_interp(struct child_info *child, const char *path, char filename[PATH_MAX], char **argv[])
{
	char interpreter[PATH_MAX];
	char interp[PATH_MAX] = { '\0' };


	union elf_header elf_header;
	union program_header program_header;

	int status;
	int fd;
	int i;

	uint64_t elf_phoff;
	uint16_t elf_phentsize;
	uint16_t elf_phnum;

	uint64_t segment_offset;
	uint64_t segment_size;

	/* Read the ELF header. */
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -errno;

	status = read(fd, &elf_header, sizeof(elf_header));
	if (status != sizeof(elf_header)) {
		status = -EACCES;
		goto end;
	}

	/* Check if it is an ELF file. */
	if (   ELF_IDENT(elf_header, 0) != 0x7f
	    || ELF_IDENT(elf_header, 1) != 'E'
	    || ELF_IDENT(elf_header, 2) != 'L'
	    || ELF_IDENT(elf_header, 3) != 'F') {
		status = -EACCES;
		goto end;
	}

	/* Check if it is a known class (32-bit or 64-bit). */
	if (   !IS_CLASS32(elf_header)
	    && !IS_CLASS64(elf_header)) {
		status = -EACCES;
		goto end;
	}

	/* Get class-specific fields. */
	elf_phoff     = ELF_FIELD(elf_header, phoff);
	elf_phentsize = ELF_FIELD(elf_header, phentsize);
	elf_phnum     = ELF_FIELD(elf_header, phnum);

	/*
	 * Some sanity checks regarding the current
	 * support of the ELF specification in PRoot.
	 */

	if (elf_phnum >= 0xffff) {
		notice(WARNING, INTERNAL, "%s: big PH tables are not yet supported.", path);
		status = -EACCES;
		goto end;
	}

	if (!KNOWN_PHENTSIZE(elf_header, elf_phentsize)) {
		notice(WARNING, INTERNAL, "%s: unsupported size of program header.", path);
		status = -EACCES;
		goto end;
	}

	/*
	 * Search the INTERP entry into the program header table.
	 */

	status = (int) lseek(fd, elf_phoff, SEEK_SET);
	if (status < 0) {
		status = -EACCES;
		goto end;
	}

	for (i = 0; i < elf_phnum; i++) {
		status = read(fd, &program_header, elf_phentsize);
		if (status != elf_phentsize) {
			status = -EACCES;
			goto end;
		}

		if (!IS_INTERP(elf_header, program_header))
			continue;

		/*
		 * Found the INTERP entry.
		 */

		segment_offset = PROGRAM_FIELD(elf_header, program_header, offset);
		segment_size   = PROGRAM_FIELD(elf_header, program_header, filesz);

		if (segment_size >= sizeof(interp) - 1) {
			status = -EACCES;
			goto end;
		}

		status = (int) lseek(fd, segment_offset, SEEK_SET);
		if (status < 0) {
			status = -EACCES;
			goto end;
		}

		status = read(fd, interp, segment_size);
		if (status < 0) {
			status = -EACCES;
			goto end;
		}
		interp[segment_size] = '\0';
	}

	if (interp[0] == '\0') {
		status = 0;
		goto end;
	}

	/* Special invocation to check if an ELF interpreter use
	 * an[other] ELF interpreter, this is a sanity check made by
	 * execve.c:expand_interp(). */
	if (filename == NULL)
		return -EPERM;

	/*
	 * Prepend the ELF interpreter just like we do for script
	 * interpreters:
	 *
	 *     execve("/bin/ls", argv, = [ "-l" , ... ], envp);
	 *
	 * becomes:
	 *
	 *     execve("/lib/ld-linux.so", argv, = [ "/bin/ls", "-l" , ... ], envp);
	 */

	status = translate_path(child, interpreter, AT_FDCWD, interp, REGULAR);
	if (status < 0)
		goto end;

	VERBOSE(3, "expand ELF .interp: %s -> %s %s",
		(*argv)[0], interpreter, filename);

	status = substitute_argv0(argv, 2, interpreter, filename);
	if (status < 0)
		goto end;

	/* Remember the actual executable since /proc/self/exe points
	 * to the ELF interpreter instead. */
	child->exe = strdup(path);

	strcpy(filename, interpreter);
	status = 0;
end:
	close(fd);

	/* Delayed error handling */
	if (status < 0)
		return status;

	/* Is there an INTERP entry? */
	if (interp[0] == '\0')
		return 0;
	else
		return 1;
}
