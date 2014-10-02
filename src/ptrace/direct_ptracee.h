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

#ifndef PTRACE_DIRECT_PTRACEE_H
#define PTRACE_DIRECT_PTRACEE_H

#include "tracee/tracee.h"

extern int add_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern bool is_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern bool is_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern void remove_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);
extern void set_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid);

#endif /* PTRACE_DIRECT_PTRACEE_H */
