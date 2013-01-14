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

#include <errno.h>  /* errno, */
#include <string.h> /* strerror(3), */
#include <stdarg.h> /* va_*, */
#include <stdio.h>  /* vfprintf(3), */
#include <limits.h> /* INT_MAX, */

#include "notice.h"
#include "tracee/tracee.h"

/**
 * Print @message to the standard error stream according to its
 * @severity and @origin.
 */
void notice(const Tracee *tracee, Severity severity, Origin origin, const char *message, ...)
{
	va_list extra_params;
	int verbose_level;

	verbose_level = (tracee != NULL ? tracee->verbose : 0);

	if (verbose_level < 0 && severity != ERROR)
		return;

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

	if (origin == TALLOC)
		fprintf(stderr, "talloc: ");

	va_start(extra_params, message);
	vfprintf(stderr, message, extra_params);
	va_end(extra_params);

	switch (origin) {
	case SYSTEM:
		fprintf(stderr, ": ");
		perror(NULL);
		break;

	case TALLOC:
		break;

	case INTERNAL:
	case USER:
	default:
		fprintf(stderr, "\n");
		break;
	}

	return;
}

