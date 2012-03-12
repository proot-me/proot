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

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>     /* offsetof(), */
#include <limits.h>     /* PATH_MAX, */
#include <stdbool.h>    /* bool ,true, false, */

#include "arch.h" /* word_t */
#include "tracee/info.h"

enum sysarg {
	SYSARG_NUM = 0,
	SYSARG_1,
	SYSARG_2,
	SYSARG_3,
	SYSARG_4,
	SYSARG_5,
	SYSARG_6,
	SYSARG_RESULT,

	/* Helpers. */
	SYSARG_FIRST = SYSARG_NUM,
	SYSARG_LAST  = SYSARG_RESULT
};

#define STACK_POINTER (SYSARG_LAST + 1)

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

extern int translate_syscall(pid_t pid);
extern int get_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum sysarg sysarg);
extern int set_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum sysarg sysarg);
extern int set_sysarg_data(struct tracee_info *tracee, void *tracer_ptr, word_t size, enum sysarg sysarg);

#endif /* SYSCALL_H */
