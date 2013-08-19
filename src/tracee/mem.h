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

#ifndef TRACEE_MEM_H
#define TRACEE_MEM_H

#include <limits.h>    /* PATH_MAX, */
#include <sys/types.h> /* pid_t, size_t, */
#include <sys/uio.h>   /* struct iovec, */

#include "arch.h" /* word_t, */
#include "tracee/tracee.h"

extern int write_data(const Tracee *tracee, word_t dest_tracee, const void *src_tracer, word_t size);
extern int writev_data(const Tracee *tracee, word_t dest_tracee, const struct iovec *src_tracer, int src_tracer_count);
extern int read_data(const Tracee *tracee, void *dest_tracer, word_t src_tracee, word_t size);
extern int read_string(const Tracee *tracee, char *dest_tracer, word_t src_tracee, word_t max_size);
extern word_t peek_mem(const Tracee *tracee, word_t address);
extern void poke_mem(const Tracee *tracee, word_t address, word_t value);
extern word_t alloc_mem(Tracee *tracee, ssize_t size);

/**
 * Copy to @dest_tracer at most PATH_MAX bytes -- including the
 * end-of-string terminator -- from the string pointed to by
 * @src_tracee within the memory space of the @tracee process.  This
 * function returns -errno on error, otherwise it returns the number
 * in bytes of the string, including the end-of-string terminator.
 */
static inline int read_path(const Tracee *tracee, char dest_tracer[PATH_MAX], word_t src_tracee)
{
	int status;

	status = read_string(tracee, dest_tracer, src_tracee, PATH_MAX);
	if (status < 0)
		return status;
	if (status >= PATH_MAX)
		return -ENAMETOOLONG;

	return status;
}

#endif /* TRACEE_MEM_H */
