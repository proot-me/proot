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

#ifndef TRACEE_INFO_H
#define TRACEE_INFO_H

#include <sys/types.h> /* pid_t, size_t, */
#include <stdbool.h>

#include "arch.h" /* word_t, */

/* Information related to a tracee process. */
struct tracee_info {
	pid_t  pid;    /* Process identifier. */
	word_t sysnum; /* Current syscall (-1 if none). */
	int    status; /* -errno if < 0, otherwise amount of bytes used in the tracee's stack. */
	char *trigger; /* Name of the file/directory used to start the path translation. */
	off_t *uregs;  /* Current register bank, also used to know the current ABI. */
	char *exe;     /* Path to the executable, à la /proc/self/exe. */
};

typedef int (*foreach_tracee_t)(pid_t pid);

extern void init_module_tracee_info(void);
extern struct tracee_info *new_tracee(pid_t pid);
extern void delete_tracee(pid_t pid);
extern size_t get_nb_tracees(void);
extern struct tracee_info *get_tracee_info(pid_t pid);
extern int foreach_tracee(foreach_tracee_t callback);

#endif /* TRACEE_INFO_H */
