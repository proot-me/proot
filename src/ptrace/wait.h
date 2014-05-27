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

#ifndef PTRACE_WAIT_H
#define PTRACE_WAIT_H

#include "tracee/tracee.h"

extern int translate_wait_enter(Tracee *ptracer);
extern int translate_wait_exit(Tracee *ptracer);
extern bool handle_ptracee_event(Tracee *ptracee, int wait_status);

extern int add_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern bool is_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern void set_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern bool is_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern void remove_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);

/* __WCLONE: Wait for "clone" children only.  If omitted then wait for
 * "non-clone" children only.  (A "clone" child is one which delivers
 * no signal, or a signal other than SIGCHLD to its parent upon
 * termination.)  This option is ignored if __WALL is also specified.
 *
 * __WALL: Wait for all children, regardless of type ("clone" or
 * "non-clone").
 *
 * -- wait(2) man-page
 */
#define EXPECTED_WAIT_CLONE(wait_options, tracee)		\
	((((wait_options) & __WALL) != 0)			\
      || ((((wait_options) & __WCLONE) != 0) && (tracee)->clone) \
      || ((((wait_options) & __WCLONE) == 0) && !(tracee)->clone))

#endif /* PTRACE_WAIT_H */
