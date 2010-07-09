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
 * Inspired by: strace and QEMU user-mode.
 */

#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <sys/types.h>  /* pid_t, */
#include <stdlib.h>     /* NULL, */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <errno.h>      /* errno, */
#include <stdio.h>      /* perror(3), fprintf(3), */

#include "syscall.h" /* enum arg_number, ARG_*, */
#include "arch.h"    /* REG_ARG_* */

/* Specify the offset in the USER area of each register used for
 * syscall argument passing. */
#define USER_REGS_OFFSET(reg_name) (offsetof(struct user, regs) + offsetof(struct user_regs_struct, reg_name))
size_t arg_offset[] = {
	USER_REGS_OFFSET(REG_ARG_SYSNUM),
	USER_REGS_OFFSET(REG_ARG_1),
	USER_REGS_OFFSET(REG_ARG_2),
	USER_REGS_OFFSET(REG_ARG_3),
	USER_REGS_OFFSET(REG_ARG_4),
	USER_REGS_OFFSET(REG_ARG_5),
	USER_REGS_OFFSET(REG_ARG_6),
	USER_REGS_OFFSET(REG_ARG_RESULT),
};

/**
 * Return the @arg_number-th argument of the current syscall in the
 * @pid process.
 */
unsigned long get_syscall_arg(pid_t pid, enum arg_number arg_number)
{
	unsigned long result;

	if (arg_number < ARG_FIRST || arg_number > ARG_LAST) {
		fprintf(stderr, "proot -- set_syscall_arg(%d) not supported\n", arg_number);
		exit(EXIT_FAILURE);
	}

	result = ptrace(PTRACE_PEEKUSER, pid, arg_offset[arg_number], NULL);
        if (errno != 0) {
                perror("proot -- ptrace(PEEKUSER)");
                exit(EXIT_FAILURE);
        }

	return result;
}

/**
 * Set the @arg_number-th argument of the current syscall in the @pid
 * process to @value.
 */
void set_syscall_arg(pid_t pid, enum arg_number arg_number, unsigned long value)
{
	unsigned long status = 0;

	if (arg_number < ARG_FIRST || arg_number > ARG_LAST) {
		fprintf(stderr, "proot -- set_syscall_arg(%d) not supported\n", arg_number);
		exit(EXIT_FAILURE);
	}

	status = ptrace(PTRACE_POKEUSER, pid, arg_offset[arg_number], value);
        if (status < 0) {
                perror("proot -- ptrace(POKEUSER)");
                exit(EXIT_FAILURE);
        }
}
