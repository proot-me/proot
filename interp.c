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

#include "interp.h"
#include "path.h"
#include "execve.h"
#include "notice.h"

/**
 * Expand the shebang of @path in *@argv[]. This function returns
 * -errno if an error occured, 1 if a shebang was found and expanded,
 * otherwise 0. @path is updated to point to the detranslated path
 * (interpreter or script).
 *
 * Extract from "man 2 execve":
 *
 *     On Linux, the entire string following the interpreter name is
 *     passed as a *single* argument to the interpreter, and this
 *     string can include white space.
 */
int expand_script_interp(struct child_info *child, char path[PATH_MAX], char **argv[])
{
	char interpreter[PATH_MAX];
	char argument[ARG_MAX];
	char tmp;

	int status2;
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
	if (status < 0)
		return status;

	status2 = detranslate_path(path, 1);
	if (status2 < 0)
		return status2;

	switch (status) {
	case 0:
		/* Does this file use a script interpreter? */
		return 0;

	case 1:
		VERBOSE(3, "expand shebang: %s -> %s %s",
			(*argv)[0], interpreter, path);

		status = substitute_argv0(argv, 2, interpreter, path);
		break;

	case 2:
		VERBOSE(3, "expand shebang: %s -> %s %s %s",
			(*argv)[0], interpreter, argument, path);

		status = substitute_argv0(argv, 3, interpreter, argument, path);
		break;

	default:
		assert(0);
		break;
	}
	if (status < 0)
		return status;

	/* Inform the caller about the program to execute. */
	strcpy(path, interpreter);

	return 1;
}

/**
 * Expand the shebang of @filename in *@argv[]. This function returns
 * -errno if an error occured, 1 if a ELF interpreter was found and
 * expanded, otherwise 0.
 */
int expand_elf_interp(struct child_info *child, char path[PATH_MAX], char **argv[])
{
	return 0;
}
