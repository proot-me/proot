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

#include <limits.h>    /* PATH_MAX, */
#include <sys/types.h> /* pid_t, size_t, */

#include "arch.h" /* word_t, */

/* Information related to a child process. */
struct child_info {
	pid_t  pid;    /* Process identifier. */
	word_t sysnum; /* Current syscall (-1 if none). */
	int    status; /* -errno if < 0, otherwise amount of bytes used in the child's stack. */
	word_t output; /* Address in the child's memory space of the output argument. */
};

extern void init_children_info(size_t nb_elements);
extern int trace_new_child(pid_t pid, int on_the_fly);
extern void delete_child(pid_t pid);
extern size_t get_nb_children();
extern struct child_info *get_child_info(pid_t pid);

extern word_t resize_child_stack(pid_t pid, ssize_t size);
extern int copy_to_child(pid_t pid, word_t dest_child, const void *src_parent, word_t size);
extern int copy_from_child(pid_t pid, void *dest_parent, word_t src_child, word_t size);
extern int get_child_string(pid_t pid, void *dest_parent, word_t src_child, word_t max_size);

#endif /* CHILD_H */
