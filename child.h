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
 */

#ifndef CHILD_H
#define CHILD_H

enum sysarg {
	/* Warning: sysarg_offset[] in child.c relies on this ordering. */
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

extern unsigned long get_child_sysarg(pid_t pid, enum sysarg sysarg);
extern void set_child_sysarg(pid_t pid, enum sysarg sysarg, unsigned long value);

extern void *alloc_child_stack(pid_t pid, size_t size);
extern void free_child_stack(pid_t pid, void *buffer, size_t size);

extern void copy_to_child(pid_t pid, void *to_child, const void *from, unsigned long nb_bytes);
extern unsigned long get_child_string(pid_t pid, void *to, const void *from_child, unsigned long nb_bytes);

extern void copy_to_child(pid_t pid, void *dest_child, const void *src_parent, unsigned long size);
extern unsigned long get_child_string(pid_t pid, void *dest_parent, const void *src_child, unsigned long max_size);

#endif /* CHILD_H */
