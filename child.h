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

#include <limits.h> /* PATH_MAX, */

#include "arch.h" /* word_t, */

extern word_t resize_child_stack(pid_t pid, ssize_t size);
extern int copy_to_child(pid_t pid, word_t dest_child, const void *src_parent, word_t size);
extern int copy_from_child(pid_t pid, void *dest_parent, word_t src_child, word_t size);
extern word_t get_child_string(pid_t pid, void *dest_parent, word_t src_child, word_t max_size);
extern int get_child_cwd(pid_t pid, char cwd[PATH_MAX]);

#endif /* CHILD_H */
