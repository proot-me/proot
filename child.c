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
#include <sys/types.h>  /* pid_t, size_t, */
#include <stdlib.h>     /* NULL, exit(3), */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <errno.h>      /* errno, */
#include <stdio.h>      /* perror(3), fprintf(3), */
#include <limits.h>     /* ULONG_MAX, */
#include <assert.h>     /* assert(3), */
#include <sys/wait.h>   /* waitpid(2), */

#include "child.h"
#include "arch.h"    /* REG_SYSARG_*, word_t */
#include "syscall.h" /* USER_REGS_OFFSET, */

static struct child_info *children_info;
static size_t max_children = 0;
static size_t nb_children = 0;

/**
 * Reset the default values for the structure @child.
 */
static void reset_child(struct child_info *child)
{
	child->pid    = 0;
	child->sysnum = -1;
	child->status = 0;
	child->output = 0;
}

/**
 * Allocate @nb_elements empty entries in the table children_info[].
 */
void init_children_info(size_t nb_elements)
{
	size_t i;

	assert(nb_elements > 0);

	children_info = calloc(nb_elements, sizeof(struct child_info));
	if (children_info == NULL) {
		perror("proot -- calloc()");
		exit(EXIT_FAILURE);
	}

	/* Set the default values for each entry. */
	for(i = 0; i < nb_elements; i++)
		reset_child(&children_info[i]);

	max_children = nb_elements;
}

/**
 * Initialize a new entry in the table children_info[] for the child @pid.
 */
struct child_info *new_child(pid_t pid)
{
	size_t i;
	size_t first_slot;
	struct child_info *new_children_info;

	assert(nb_children <= max_children);

	/* Check if there is still an empty entry in child_info[]. */
	if (nb_children == max_children) {
		new_children_info = realloc(children_info, 2 * max_children * sizeof(struct child_info));
		if (new_children_info == NULL) {
			perror("proot -- realloc()");
			return NULL;
		}

		/* Set the default values for each new entry. */
		for(i = max_children; i < 2 * max_children; i++)
			reset_child(&new_children_info[i]);

		first_slot = max_children; /* Skip non-empty slot. */
		max_children = 2 * max_children;
		children_info = new_children_info;
	}
	else
		first_slot = 0;

	/* Search for a free slot. */
	for(i = first_slot; i < max_children; i++) {
		if (children_info[i].pid == 0) {
			children_info[i].pid = pid;
			nb_children++;
			return &children_info[i];
		}
	}

	/* Should never happen. */
	return NULL;
}

/**
 * Reset the entry in children_info[] related to the child @pid.
 */
void delete_child(pid_t pid)
{
	struct child_info *child;

	nb_children--;

	child = get_child_info(pid);
	reset_child(child);
}

/**
 * Give the number of child alive at this time.
 */
size_t get_nb_children()
{
	return nb_children;
}

/**
 * Search in the table children_info[] for the entry related to the
 * child @pid.
 */
struct child_info *get_child_info(pid_t pid)
{
	size_t i;

	/* Search for the entry related to this child process. */
	for(i = 0; i < max_children; i++)
		if (children_info[i].pid == pid)
			return &children_info[i];

	/* Create the children_info[] entry dynamically. */
	return new_child(pid);
}

/**
 * Call @callback on each living child process. It returns the status
 * of the first failure, that is, if @callback returned seomthing
 * lesser than 0, otherwise 0.
 */
int foreach_child(foreach_child_t callback)
{
	int status;
	int i;

	for(i = 0; i < max_children; i++) {
		if (children_info[i].pid == 0)
			continue;

		status = callback(children_info[i].pid);
		if (status < 0)
			return status;
	}

	return 0;
}


/**
 * Resize by @size bytes the stack of the child @pid. This function
 * returns 0 if an error occured, otherwise it returns the address of
 * the new stack pointer within the child's memory space.
 */
word_t resize_child_stack(pid_t pid, ssize_t size)
{
	word_t stack_pointer;
	long status;

	/* Get the current value of the stack pointer from the child's
	 * USER area. */
	status = ptrace(PTRACE_PEEKUSER, pid, USER_REGS_OFFSET(REG_SP), NULL);
	if (errno != 0)
		return 0;

	stack_pointer = (word_t)status;

	/* Sanity check. */
	if (   (size > 0 && stack_pointer <= size)
	    || (size < 0 && stack_pointer >= ULONG_MAX + size)) {
		fprintf(stderr, "proot -- integer under/overflow detected in %s\n", __FUNCTION__);
		return 0;
	}

	/* Remember the stack grows downward. */
	stack_pointer -= size;

	/* Set the new value of the stack pointer in the child's USER
	 * area. */
	status = ptrace(PTRACE_POKEUSER, pid, USER_REGS_OFFSET(REG_SP), stack_pointer);
	if (status < 0)
		return 0;

	return stack_pointer;
}

/**
 * Copy @size bytes from the buffer @src_parent to the address
 * @dest_child within the memory space of the child process @pid. It
 * return -errno if an error occured, otherwise 0.
 */
int copy_to_child(pid_t pid, word_t dest_child, const void *src_parent, word_t size)
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
			return -EFAULT;
		}
	}

	/* Copy the bytes in the last word carefully since we have
	 * overwrite only the relevant ones. */

	word = ptrace(PTRACE_PEEKDATA, pid, dest + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		return -EFAULT;
	}

	last_dest_word = (unsigned char *)&word;
	last_src_word  = (unsigned char *)&src[i];

	for (j = 0; j < nb_trailing_bytes; j++)
		last_dest_word[j] = last_src_word[j];

	status = ptrace(PTRACE_POKEDATA, pid, dest + i, word);
	if (status < 0) {
		perror("proot -- ptrace(POKEDATA)");
		return -EFAULT;
	}

	return 0;
}

/**
 * Copy @size bytes to the buffer @dest_parent from the address
 * @src_child within the memory space of the child process @pid. It
 * return -errno if an error occured, otherwise 0.
 */
int copy_from_child(pid_t pid, void *dest_parent, word_t src_child, word_t size)
{
	word_t *src  = (word_t *)src_child;
	word_t *dest = (word_t *)dest_parent;

	word_t nb_trailing_bytes;
	word_t nb_full_words;
	word_t word, i, j;

	unsigned char *last_src_word;
	unsigned char *last_dest_word;

	nb_trailing_bytes = size % sizeof(word_t);
	nb_full_words     = (size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
		if (errno != 0) {
			perror("proot -- ptrace(PEEKDATA)");
			return -EFAULT;
		}
		dest[i] = word;
	}

	/* Copy the bytes from the last word carefully since we have
	 * to not overwrite the bytes lying beyond the @to buffer. */

	word = ptrace(PTRACE_PEEKDATA, pid, src + i, NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKDATA)");
		return -EFAULT;
	}

	last_dest_word = (unsigned char *)&dest[i];
	last_src_word  = (unsigned char *)&word;

	for (j = 0; j < nb_trailing_bytes; j++)
		last_dest_word[j] = last_src_word[j];

	return 0;
}

/**
 * Copy to @dest_parent at most @max_size bytes from the string
 * pointed to by @src_child within the memory space of the child
 * process @pid. This function returns -errno on error, otherwise
 * it returns the number in bytes of the string, including the
 * end-of-string terminator.
 */
int get_child_string(pid_t pid, void *dest_parent, word_t src_child, word_t max_size)
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
		if (errno != 0)
			return -EFAULT;

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
	if (errno != 0)
		return -EFAULT;

	dest_word = (unsigned char *)&dest[i];
	src_word  = (unsigned char *)&word;

	for (j = 0; j < nb_trailing_bytes; j++) {
		dest_word[j] = src_word[j];
		if (src_word[j] == '\0')
			break;
	}

	return i * sizeof(word_t) + j + 1;
}
