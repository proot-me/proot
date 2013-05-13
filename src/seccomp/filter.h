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

#ifndef SECCOMP_FILTER_H
#define SECCOMP_FILTER_H

#include <linux/filter.h>  /* struct sock_*, */
#include <sys/types.h>     /* size_t, */
#include <stdint.h>        /* uint*_t, */

#include "arch.h"

extern int new_filter(struct sock_fprog *program);
extern int start_filter_arch_section(struct sock_fprog *program, uint32_t arch,
				size_t nb_traced_syscalls);
extern int add_filter_trace_syscall(struct sock_fprog *program, word_t syscall, int flag);
extern int end_filter_arch_section(struct sock_fprog *program, size_t nb_traced_syscalls);
extern int finalize_filter(struct sock_fprog *program);
extern int free_filter(struct sock_fprog *program);

#endif  /* SECCOMP_FILTER_H */
