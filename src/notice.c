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

#include <errno.h>  /* errno, */
#include <string.h> /* strerror(3), */
#include <stdlib.h> /* exit(3), EXIT_*, */
#include <stdarg.h> /* va_*, */
#include <stdio.h>  /* vfprintf(3), */

#include "notice.h"

/**
 * XXX
 */
void notice(enum notice_severity severity, enum notice_origin origin, const char *message, ...)
{
	va_list extra_params;

	switch (severity) {
	case WARNING:
		fprintf(stderr, "proot warning: ");
		break;

	case ERROR:
		fprintf(stderr, "proot error: ");
		break;

	case INFO:
	default:
		fprintf(stderr, "proot info: ");
		break;
	}

	va_start(extra_params, message);
	vfprintf(stderr, message, extra_params);
	va_end(extra_params);

	switch (origin) {
	case SYSTEM:
		fprintf(stderr, ": ");
		perror(NULL);
		break;

	case INTERNAL:
	case USER:
	default:
		fprintf(stderr, "\n");
		break;
	}

	if (severity == ERROR) {
		fprintf(stderr, "proot error: see `proot --help` or `man proot`.\n");
		exit(EXIT_FAILURE);
	}

	return;
}

