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

#ifndef PATH_H
#define PATH_H

#include <sys/types.h> /* pid_t, */
#include <fcntl.h> /* AT_FDCWD, */
#include <limits.h> /* PATH_MAX, */
#include <stdbool.h>

#include "tracee/info.h"

/* Helper macros. */
#define REGULAR 1
#define SYMLINK 0

#define STRONG  1
#define WEAK    0

extern void init_module_path();
extern int translate_path(struct tracee_info *tracee, char result[PATH_MAX], int dir_fd, const char *fake_path, int deref_final);
extern int detranslate_path(struct tracee_info *tracee, char path[PATH_MAX], const char t_referrer[PATH_MAX]);
extern bool belongs_to_guestfs(const char *path);

extern int join_paths(int number_paths, char result[PATH_MAX], ...);
extern int next_component(char component[NAME_MAX], const char **cursor);

#define FINAL_NORMAL    1
#define FINAL_FORCE_DIR 2
extern void pop_component(char *path);

extern int check_fd(pid_t pid);
extern int list_open_fd(pid_t pid);

extern char root[PATH_MAX];
extern size_t root_length;

enum path_comparison {
	PATHS_ARE_EQUAL = 0,
	PATH1_IS_PREFIX = -1,
	PATH2_IS_PREFIX = 1,
	PATHS_ARE_NOT_COMPARABLE = 125,
};

extern enum path_comparison compare_paths(const char *path1, const char *path2);
extern enum path_comparison compare_paths2(const char *path1, size_t length1,
					const char *path2, size_t length2);

/* Check if path interpretable relatively to dirfd, see openat(2) for details. */
#define AT_FD(dirfd, path) ((dirfd) != AT_FDCWD && ((path) != NULL && (path)[0] != '/'))

#endif /* PATH_H */
