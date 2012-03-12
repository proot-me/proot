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

#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <sys/types.h>  /* pid_t, size_t, */
#include <stdlib.h>     /* NULL, exit(3), */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <limits.h>     /* ULONG_MAX, */
#include <assert.h>     /* assert(3), */
#include <sys/wait.h>   /* waitpid(2), */
#include <string.h>     /* bzero(3), */
#include <stdbool.h>    /* bool, true, false, */

#include "tracee/info.h"
#include "arch.h"    /* REG_SYSARG_*, word_t */
#include "syscall/syscall.h" /* USER_REGS_OFFSET, */
#include "notice.h"

static struct tracee_info *tracee_infos;
static size_t max_tracees = 0;
static size_t nb_tracees = 0;

/**
 * Reset the default values for the structure @tracee.
 */
static void reset_tracee(struct tracee_info *tracee, bool free_fields)
{
	if (free_fields && tracee->trigger != NULL)
		free(tracee->trigger);

	if (free_fields && tracee->exe != NULL)
		free(tracee->exe);

	bzero(tracee, sizeof(struct tracee_info));

	tracee->sysnum = -1;
}

/**
 * Allocate @nb_elements empty entries in the table tracee_infos[].
 */
void init_module_tracee_info()
{
	size_t i;
	const int nb_elements = 64;

	tracee_infos = calloc(nb_elements, sizeof(struct tracee_info));
	if (tracee_infos == NULL)
		notice(ERROR, SYSTEM, "calloc()");

	/* Set the default values for each entry. */
	for(i = 0; i < nb_elements; i++)
		reset_tracee(&tracee_infos[i], false);

	max_tracees = nb_elements;
}

/**
 * Initialize a new entry in the table tracee_infos[] for the tracee @pid.
 */
struct tracee_info *new_tracee(pid_t pid)
{
	size_t i;
	size_t first_slot;
	struct tracee_info *new_tracee_infos;

	assert(nb_tracees <= max_tracees);

	/* Check if there is still an empty entry in tracee_info[]. */
	if (nb_tracees == max_tracees) {
		new_tracee_infos = realloc(tracee_infos, 2 * max_tracees * sizeof(struct tracee_info));
		if (new_tracee_infos == NULL) {
			notice(WARNING, SYSTEM, "realloc()");
			return NULL;
		}

		/* Set the default values for each new entry. */
		for(i = max_tracees; i < 2 * max_tracees; i++)
			reset_tracee(&new_tracee_infos[i], false);

		first_slot = max_tracees; /* Skip non-empty slot. */
		max_tracees = 2 * max_tracees;
		tracee_infos = new_tracee_infos;
	}
	else
		first_slot = 0;

	/* Search for a free slot. */
	for(i = first_slot; i < max_tracees; i++) {
		if (tracee_infos[i].pid == 0) {
			tracee_infos[i].pid = pid;
			nb_tracees++;
			return &tracee_infos[i];
		}
	}

	/* Should never happen. */
	assert(0);
	return NULL;
}

/**
 * Reset the entry in tracee_infos[] related to the tracee @pid.
 */
void delete_tracee(pid_t pid)
{
	struct tracee_info *tracee;

	nb_tracees--;

	tracee = get_tracee_info(pid);
	assert(tracee != NULL);

	reset_tracee(tracee, true);
}

/**
 * Give the number of tracee alive at this time.
 */
size_t get_nb_tracees()
{
	return nb_tracees;
}

/**
 * Search in the table tracee_infos[] for the entry related to the
 * tracee @pid.
 */
struct tracee_info *get_tracee_info(pid_t pid)
{
	size_t i;

	/* Search for the entry related to this tracee process. */
	for(i = 0; i < max_tracees; i++)
		if (tracee_infos[i].pid == pid)
			return &tracee_infos[i];

	/* Create the tracee_infos[] entry dynamically. */
	return new_tracee(pid);
}

/**
 * Call @callback on each living tracee process. It returns the status
 * of the first failure, that is, if @callback returned seomthing
 * lesser than 0, otherwise 0.
 */
int foreach_tracee(foreach_tracee_t callback)
{
	int status;
	int i;

	for(i = 0; i < max_tracees; i++) {
		if (tracee_infos[i].pid == 0)
			continue;

		status = callback(tracee_infos[i].pid);
		if (status < 0)
			return status;
	}

	return 0;
}
