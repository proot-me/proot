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

enum reg_version {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
};

/* Information related to a tracee process. */
struct tracee {
	/* Process identifier. */
	pid_t pid;

	/* Path to the executable, à la /proc/self/exe. */
	char *exe;

	/* Current status:
	 *        0: enter syscall
	 *        1: exit syscall no error
	 *   -errno: exit syscall with error.  */
	int status;

	/* Value of the tracee's general purpose registers.  */
	struct user_regs_struct _regs[NB_REG_VERSION];
	bool _regs_have_changed;

	enum {         /* State for the special handling of SIGSTOP.  */
		SIGSTOP_IGNORED = 0,  /* Ignore SIGSTOP (once the parent is known).  */
		SIGSTOP_ALLOWED,      /* Allow SIGSTOP (once the parent is known).   */
		SIGSTOP_PENDING,      /* Block SIGSTOP until the parent is unknown.  */
	} sigstop;
	struct tracee *parent; /* Parent of this tracee. */
};

typedef int (*foreach_tracee_t)(struct tracee *tracee);

extern void init_module_tracee(void);
extern void free_tracees(void);

extern void delete_tracee(struct tracee *tracee);
extern struct tracee *get_tracee(pid_t pid, bool create);
extern int foreach_tracee(foreach_tracee_t callback);
extern void inherit_fs_info(struct tracee *child, struct tracee *parent);

#endif /* TRACEE_H */
