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
#include "arch.h"    /* REG_SYSARG_*, word_t */

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
word_t get_child_sysarg(pid_t pid, enum sysarg sysarg)
{
	word_t result;

	/* Sanity check. */
	if (sysarg < SYSARG_FIRST || sysarg > SYSARG_LAST) {
		fprintf(stderr, "proot -- %s(%d) not supported\n", __FUNCTION__, sysarg);
		exit(EXIT_FAILURE);
	}

	/* Get the argument register from the child's USER area. */
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
void set_child_sysarg(pid_t pid, enum sysarg sysarg, word_t value)
{
	long status;

	/* Sanity check. */
	if (sysarg < SYSARG_FIRST || sysarg > SYSARG_LAST) {
		fprintf(stderr, "proot -- %s(%d) not supported\n", __FUNCTION__, sysarg);
		exit(EXIT_FAILURE);
	}

	/* Set the argument register in the child's USER area. */
	status = ptrace(PTRACE_POKEUSER, pid, arg_offset[sysarg], value);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}
}

/**
 * This function resize by @size bytes the stack of the child @pid,
 * then it returns the address of the new stack pointer within the
 * child's memory space.
 */
word_t resize_child_stack(pid_t pid, ssize_t size)
{
	word_t stack_pointer;
	long status;

	/* Get the current value of the stack pointer
	 * from the child's USER area. */
	status = ptrace(PTRACE_PEEKUSER, pid, USER_REGS_OFFSET(REG_SP), NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKUSER)");
		exit(EXIT_FAILURE);
	}
	stack_pointer = (word_t)status;

	/* Sanity check. */
	if (   (size > 0 && stack_pointer <= size)
	    || (size < 0 && stack_pointer >= ULONG_MAX + size)) {
		fprintf(stderr, "proot -- integer underflow detected in %s\n", __FUNCTION__);
		exit(EXIT_FAILURE);
	}

	/* Remember the stack grows downward. */
	stack_pointer -= size;

	/* Set the new value of the stack pointer
	 * in the child's USER area. */
	status = ptrace(PTRACE_POKEUSER, pid, USER_REGS_OFFSET(REG_SP), stack_pointer);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}

	return stack_pointer;
}

/**
 * Copy @size bytes from the buffer @src_parent to the address
 * @dest_child within the memory space of the child process @pid.
 */
void copy_to_child(pid_t pid, word_t dest_child, const void *src_parent, word_t size)
{
	word_t *src  = (word_t *)src_parent;
	word_t *dest = (word_t *)dest_child;

	long   status;
	word_t word, i, j;
	word_t nb_trailing_bytes;
	word_t nb_full_words;

	unsigned char *last_dest_word;
	unsigned char *last_src_word;

	nb_trailing_bytes = size % sizeof(word_t);
	nb_full_words     = (size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		status = ptrace(PTRACE_POKEDATA, pid, dest + i, src[i]);
		if (status < 0) {
			perror("proot -- ptrace(POKEDATA)");
			exit(EXIT_FAILURE);
		}
	}

	/* Copy the bytes in the last word carefully since we have
	 * overwrite only the relevant ones. */

	word = ptrace(PTRACE_PEEKDATA, pid, dest + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		exit(EXIT_FAILURE);
	}

	last_dest_word = (unsigned char *)&word;
	last_src_word  = (unsigned char *)&src[i];

	for (j = 0; j < nb_trailing_bytes; j++)
		last_dest_word[j] = last_src_word[j];

	status = ptrace(PTRACE_POKEDATA, pid, dest + i, word);
	if (status < 0) {
		perror("proot -- ptrace(POKEDATA)");
		exit(EXIT_FAILURE);
	}
}

/**
 * Copy to @dest_parent at most @max_size bytes from the string
 * pointed to by @src_child within the memory space of the child
 * process @pid. This function returns (word_t)-1 on error, otherwise
 * it returns the number in bytes of the string, including the
 * end-of-string terminator.
 */
word_t get_child_string(pid_t pid, void *dest_parent, word_t src_child, word_t max_size)
{
	word_t *src  = (word_t *)src_child;
	word_t *dest = (word_t *)dest_parent;

	word_t nb_trailing_bytes;
	word_t nb_full_words;
	word_t word, i, j;

	unsigned char *src_word;
	unsigned char *dest_word;

	nb_trailing_bytes = max_size % sizeof(word_t);
	nb_full_words     = (max_size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
		if (errno != 0) {
			perror("proot -- ptrace(PEEKDATA)");
			return (word_t)-1;
		}
		dest[i] = word;

		/* Stop once an end-of-string is detected. */
		src_word = (unsigned char *)&word;
		for (j = 0; j < sizeof(word_t); j++)
			if (src_word[j] == '\0')
				return i * sizeof(word_t) + j + 1;
	}

	/* Copy the bytes from the last word carefully since we have
	 * to not overwrite the bytes lying beyond the @to buffer. */

	word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		return (word_t)-1;
	}

	dest_word = (unsigned char *)&dest[i];
	src_word  = (unsigned char *)&word;

	for (j = 0; j < nb_trailing_bytes; j++) {
		dest_word[j] = src_word[j];
		if (src_word[j] == '\0')
			break;
	}

	return i * sizeof(word_t) + j + 1;
}
