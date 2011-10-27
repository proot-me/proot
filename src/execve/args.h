/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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

#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include "syscall/syscall.h" /* enum sysarg */

#ifndef ARG_MAX
#define ARG_MAX 131072
#endif

extern void init_runner_args(const char *runner);
extern int push_env(char **envp[], const char *env, char old_env[ARG_MAX]);
extern int push_args(bool replace_argv0, char **argv[], int nb_new_args, ...);
extern int insert_runner_args(char **argv[]);
extern int get_args(struct tracee_info *tracee, char **argv[], enum sysarg sysarg);
extern int set_args(struct tracee_info *tracee, char *argv[], enum sysarg sysarg);
extern void free_args(char *argv[]);

#endif /* ARGS_H */
