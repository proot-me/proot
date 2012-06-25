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

#include <sys/types.h>  /* pid_t, size_t, */
#include <stdlib.h>     /* NULL, */
#include <assert.h>     /* assert(3), */
#include <string.h>     /* bzero(3), */
#include <stdbool.h>    /* bool, true, false, */

#include "tracee/info.h"
#include "notice.h"

 /* Don't use too many entries since XXX all XXX when searching for a
  * new tracee.  */
#define POOL_MAX_ENTRIES 16

struct pool {
	struct tracee_info entries[POOL_MAX_ENTRIES];
	size_t nb_entries;
	struct pool *next;
};

static struct pool *first_pool = NULL;

/**
 * Reset the default values for the given @tracee.
 */
void delete_tracee(struct tracee_info *tracee)
{
	assert(tracee != NULL);

	if (tracee->exe != NULL)
		free(tracee->exe);

	bzero(tracee, sizeof(struct tracee_info));

	tracee->sysnum = -1;
}

/**
 * Allocate a new pool and initialize an entry for the tracee @pid.
 */
static struct tracee_info *new_tracee(pid_t pid)
{
	struct pool *last_pool;
	struct pool *new_pool;
	size_t i;

	/* Search for the last pool.  */
	for (last_pool = first_pool; last_pool != NULL; last_pool = last_pool->next)
		if (last_pool->next == NULL)
			break;

	new_pool = calloc(1, sizeof(struct pool));
	if (new_pool == NULL) {
		notice(WARNING, SYSTEM, "calloc()");
		return NULL;
	}

	if (last_pool != NULL)
		last_pool->next = new_pool;
	else {
		assert(first_pool == NULL);
		first_pool = new_pool;
	}

	/* Set the default values for each new entry. */
	for(i = 0; i < POOL_MAX_ENTRIES; i++)
		delete_tracee(&new_pool->entries[i]);

	new_pool->entries[0].pid = pid;
	return &new_pool->entries[0];
}

/**
 * Return the entry related to the tracee @pid.  If no entry were
 * found, a new one is created if @create is true, otherwise NULL is
 * returned.
 */
struct tracee_info *get_tracee_info(pid_t pid, bool create)
{
	struct tracee_info *tracee;
	struct pool *pool;
	size_t i;

	/* Remember what the first free slot is in the first free
	 * pool, if we have to @create a new entry.  */
	struct pool *free_pool = NULL;
	size_t free_slot = 0;

	for (pool = first_pool; pool != NULL; pool = pool->next) {
		for(i = 0; i < POOL_MAX_ENTRIES; i++) {
			if (pool->entries[i].pid == pid)
				return &pool->entries[i];

			if (!create
			    || free_pool != NULL
			    || pool->entries[i].pid != 0)
				continue;

			free_pool = pool;
			free_slot = i;
		}
	}

	if (!create)
		return NULL;

	if (free_pool != NULL) {
		tracee = &free_pool->entries[free_slot];
		tracee->pid = pid;
		return tracee;
	}
	else {
		tracee = new_tracee(pid);
		assert(tracee != NULL);
		return tracee;
	}
}

/**
 * Call @callback on each tracees.  This function returns the status
 * of the first failure, that is, if @callback returned something
 * lesser than 0, otherwise 0.
 */
int foreach_tracee(foreach_tracee_t callback)
{
	struct pool *pool;
	int status;
	size_t i;

	for (pool = first_pool; pool != NULL; pool = pool->next) {
		for(i = 0; i < POOL_MAX_ENTRIES; i++) {
			if (pool->entries[i].pid == 0)
				continue;

			status = callback(pool->entries[i].pid);
			if (status < 0)
				return status;
		}
	}

	return 0;
}

/**
 * Make the @child tracee inherit filesystem information from the
 * @parent tracee.  Depending on the @parent->clone_flags, some
 * information are copied or shared.
 */
void inherit_fs_info(struct tracee_info *child, struct tracee_info *parent)
{
	child->parent = parent;

	assert(child->exe  == NULL);
	// assert(child->root == NULL);
	// assert(child->cwd  == NULL);

	/* The first tracee is started by PRoot and does nothing but a
	 * call to execve(2), thus child->exe will be automatically
	 * updated later.  */
	if (parent == NULL) {
		child->parent = (void *)-1;
		child->exe = NULL;
		// child->root = strdup(config.guest_rootfs);
		// child->cwd  = strdup(config.initial_cwd);
		return;
	}

	assert(parent->exe  != NULL);
	// assert(parent->root != NULL);
	// assert(parent->cwd  != NULL);

	/* The path to the executable is updated if the process does a
	 * call to execve(2).  */
	child->exe = strdup(parent->exe);

#if 0
	/* If CLONE_FS is set, the parent and the child process share
	 * the same file system information.  This includes the root
	 * of the file system, the current working directory, and the
	 * umask.  Any call to chroot(2), chdir(2), or umask(2)
	 * performed by the parent process or the child process also
	 * affects the other process.
	 *
	 * If CLONE_FS is not set, the child process works on a copy
	 * of the file system information of the parent process at the
	 * time of the clone() call.  Calls to chroot(2), chdir(2),
	 * umask(2) performed later by one of the processes do not
	 * affect the other process.
	 *
	 * -- clone(2) man-page
	 */
	if ((parent->clone_flags & CLONE_FS) != 0) {
		/* File-system information is shared.  */
		child->root = parent->root;
		child->cwd  = parent->cwd;
		/* TODO: use a reference counter to release the memory
		   only once no tracee uses it.  */
	}
	else {
		/* File-system information is copied.  */
		child->root = strdup(parent->root);
		child->cwd  = strdup(parent->cwd);
	}
#endif

	return;
}
