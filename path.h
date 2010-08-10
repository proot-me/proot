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

#ifndef PATH_H
#define PATH_H

#include <sys/types.h> /* pid_t, */
#include <fcntl.h> /* AT_FDCWD, */

/* Helper macros. */
#define REGULAR 1
#define SYMLINK 0

#define STRONG  1
#define WEAK    0

extern void init_path_translator(const char *new_root);
extern int translate_path(pid_t pid, char result[PATH_MAX], const char *fake_path, int deref_final);
extern int detranslate_path(char path[PATH_MAX], int sanity_check);
extern int check_path_at(pid_t pid, int dirfd, char path[PATH_MAX], int deref_final);
extern int check_fd(pid_t pid);

/* Check if path interpretable relatively to dirfd, see openat(2) for details. */
#define AT_FD(dirfd, path) ((dirfd) != AT_FDCWD && ((path) != NULL && (path)[0] != '/'))

#endif /* PATH_H */
