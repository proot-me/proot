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

#ifndef SECCOMP_H
#define SECCOMP_H

#include "tracee/tracee.h"
#include "attribute.h"
#include "arch.h"

typedef struct {
	word_t value;
	word_t flags;
} FilteredSyscall;

typedef struct {
	int arch;
	FilteredSyscall *syscalls;
} Filter;

#define FILTERED_SYSCALL_END { SYSCALL_AVOIDER, -1 }

#define FILTER_SYSEXIT  0x1

extern int enable_syscall_filtering(const Tracee *tracee);

#endif /* SECCOMP_H */
