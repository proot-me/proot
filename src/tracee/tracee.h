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

#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h> /* pid_t, size_t, */
#include <sys/user.h>  /* struct user*, */
#include <stdbool.h>

#include "arch.h" /* word_t, user_regs_struct, */

/* Information related to a tracee process. */
struct tracee {
	pid_t  pid;    /* Process identifier. */
	word_t sysnum; /* Current syscall (-1 if none). */
	int    status; /* -errno if < 0, otherwise amount of bytes used in the tracee's stack. */
	char *exe;     /* Path to the executable, à la /proc/self/exe. */

	/* Cache for the tracee's general purpose registers.  */
	struct {
		struct user_regs_struct cache;
		enum {
			REGS_ARE_INVALID = 0,
			REGS_ARE_VALID,
			REGS_HAVE_CHANGED,
		} state;
	} _regs;

	enum {         /* State for the special handling of SIGSTOP.  */
		SIGSTOP_IGNORED = 0,  /* Ignore SIGSTOP (once the parent is known).  */
		SIGSTOP_ALLOWED,      /* Allow SIGSTOP (once the parent is known).   */
		SIGSTOP_PENDING,      /* Block SIGSTOP until the parent is unknown.  */
	} sigstop;
	struct tracee *parent; /* Parent of this tracee. */
};

typedef int (*foreach_tracee_t)(pid_t pid);

extern void init_module_tracee(void);
extern void delete_tracee(struct tracee *tracee);
extern struct tracee *get_tracee(pid_t pid, bool create);
extern int foreach_tracee(foreach_tracee_t callback);
extern void inherit_fs_info(struct tracee *child, struct tracee *parent);

#endif /* TRACEE_H */
