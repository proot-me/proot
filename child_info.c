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
#include <limits.h>     /* ULONG_MAX, */
#include <assert.h>     /* assert(3), */
#include <sys/wait.h>   /* waitpid(2), */

#include "child_info.h"
#include "arch.h"    /* REG_SYSARG_*, word_t */
#include "syscall.h" /* USER_REGS_OFFSET, */
#include "notice.h"

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
	child->trigger = NULL;
}

/**
 * Allocate @nb_elements empty entries in the table children_info[].
 */
void init_module_child_info()
{
	size_t i;
	const int nb_elements = 64;

	children_info = calloc(nb_elements, sizeof(struct child_info));
	if (children_info == NULL)
		notice(ERROR, SYSTEM, "calloc()");

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
			notice(WARNING, SYSTEM, "realloc()");
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
	assert(0);
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
	assert(child != NULL);

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
