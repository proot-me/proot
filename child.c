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
 * Inspired by: strace.
 */

#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <sys/types.h>  /* pid_t, */
#include <stdlib.h>     /* NULL, */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <errno.h>      /* errno, */
#include <stdio.h>      /* perror(3), fprintf(3), */
#include <limits.h>     /* ULONG_MAX, */

#include "child.h"
#include "arch.h"    /* REG_SYSARG_* */

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

/**
 * Specify the offset in the child's USER area of each register used
 * for syscall argument passing. */
size_t arg_offset[] = {
	USER_REGS_OFFSET(REG_SYSARG_NUM),
	USER_REGS_OFFSET(REG_SYSARG_1),
	USER_REGS_OFFSET(REG_SYSARG_2),
	USER_REGS_OFFSET(REG_SYSARG_3),
	USER_REGS_OFFSET(REG_SYSARG_4),
	USER_REGS_OFFSET(REG_SYSARG_5),
	USER_REGS_OFFSET(REG_SYSARG_6),
	USER_REGS_OFFSET(REG_SYSARG_RESULT),
};

/**
 * Return the @sysarg argument of the current syscall in the
 * child process @pid.
 */
unsigned long get_child_sysarg(pid_t pid, enum sysarg sysarg)
{
	unsigned long result;

	if (sysarg < SYSARG_FIRST || sysarg > SYSARG_LAST) {
		fprintf(stderr, "proot -- %s(%d) not supported\n", __FUNCTION__, sysarg);
		exit(EXIT_FAILURE);
	}

	result = ptrace(PTRACE_PEEKUSER, pid, arg_offset[sysarg], NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKUSER)");
		exit(EXIT_FAILURE);
	}

	return result;
}

/**
 * Set the @sysarg argument of the current syscall in the child
 * process @pid to @value.
 */
void set_child_sysarg(pid_t pid, enum sysarg sysarg, unsigned long value)
{
	unsigned long status;

	if (sysarg < SYSARG_FIRST || sysarg > SYSARG_LAST) {
		fprintf(stderr, "proot -- %s(%d) not supported\n", __FUNCTION__, sysarg);
		exit(EXIT_FAILURE);
	}

	status = ptrace(PTRACE_POKEUSER, pid, arg_offset[sysarg], value);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}
}

/**
 * This function returns an uninitialized buffer of @size bytes
 * allocated into the stack of the child process @pid.
 */
void *alloc_in_child(pid_t pid, size_t size)
{
	unsigned long stack_pointer;
	unsigned long status;

	status = ptrace(PTRACE_PEEKUSER, pid, USER_REGS_OFFSET(REG_SP), NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKUSER)");
		exit(EXIT_FAILURE);
	}

	stack_pointer = status;
	if (stack_pointer <= size) {
		fprintf(stderr, "proot -- integer underflow detected in %s", __FUNCTION__);
		exit(EXIT_FAILURE);
	}

	stack_pointer -= size;

	status = ptrace(PTRACE_POKEUSER, pid, USER_REGS_OFFSET(REG_SP), stack_pointer);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}

	return (void *)stack_pointer;
}

/**
 * This function frees the memory allocated by alloc_buffer().
 */
void free_in_child(pid_t pid, void *buffer, size_t size)
{
	unsigned long stack_pointer;
	unsigned long status;

	stack_pointer = (unsigned long)buffer;
	if (stack_pointer >= ULONG_MAX - size) {
		fprintf(stderr, "proot -- integer overflow detected in %s", __FUNCTION__);
		exit(EXIT_FAILURE);
	}

	stack_pointer += size;

	status = ptrace(PTRACE_POKEUSER, pid, USER_REGS_OFFSET(REG_SP), stack_pointer);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}

	return;
}

/**
 * Copy @nb_bytes bytes from the buffer @from into the memory space of
 * the child process @pid at its address @to_child.
 */
void copy_to_child(pid_t pid, void *to_child, const void *from, unsigned long nb_bytes)
{
	unsigned long *src = (unsigned long *)from;

	unsigned long nb_trailing_bytes;
	unsigned long nb_full_words;
	unsigned long status, word, i, j;

	unsigned char *last_dest_word;
	unsigned char *last_src_word;

	nb_trailing_bytes = nb_bytes % sizeof(unsigned long);
	nb_full_words     = (nb_bytes - nb_trailing_bytes) / sizeof(unsigned long);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		status = ptrace(PTRACE_POKEDATA, pid, to_child + i, src[i]);
		if (status < 0) {
			perror("proot -- ptrace(POKEDATA)");
			exit(EXIT_FAILURE);
		}
	}

	/* Copy the bytes in the last word carefully since I have
	 * overwrite only the relevant ones. */

	word = ptrace(PTRACE_PEEKDATA, pid, to_child + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		exit(EXIT_FAILURE);
	}

	last_dest_word = (unsigned char *)&word;
	last_src_word  = (unsigned char *)&src[i];

	for (j = 0; j < nb_trailing_bytes; j++)
		last_dest_word[j] = last_src_word[j];

	status = ptrace(PTRACE_POKEDATA, pid, to_child + i, word);
	if (status < 0) {
		perror("proot -- ptrace(POKEDATA)");
		exit(EXIT_FAILURE);
	}
}

/**
 * Copy @nb_bytes bytes into the buffer @to from the memory space of
 * the child process @pid at its address @from_child.
 */
void copy_from_child(pid_t pid, void *to, const void *from_child, unsigned long nb_bytes)
{
	unsigned long *src  = (unsigned long *)from_child;
	unsigned long *dest = (unsigned long *)to;

	unsigned long nb_trailing_bytes;
	unsigned long nb_full_words;
	unsigned long word;
	unsigned long i;

	unsigned char *last_dest_word;
	unsigned char *last_src_word;

	nb_trailing_bytes = nb_bytes % sizeof(unsigned long);
	nb_full_words     = (nb_bytes - nb_trailing_bytes) / sizeof(unsigned long);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
		if (errno != 0) {
			perror("proot -- ptrace(PEEKDATA)");
			exit(EXIT_FAILURE);
		}
		dest[i] = word;
	}

	/* Copy the bytes from the last word carefully since I have to
	 * not overwrite the bytes lying beyond the @to buffer. */

	word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		exit(EXIT_FAILURE);
	}

	last_dest_word = (unsigned char *)&dest[i];
	last_src_word  = (unsigned char *)&word;

	for (i = 0; i < nb_trailing_bytes; i++)
		last_dest_word[i] = last_src_word[i];
}
