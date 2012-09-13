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
#include <sys/queue.h>  /* LIST_*,  */
#include <talloc.h>     /* talloc_*, */

#include "tracee/tracee.h"
#include "notice.h"

struct tracees tracees;

/**
 * Remove @tracee from the list of tracees.
 */
static int remove_tracee(struct tracee *tracee)
{
	assert(tracee != NULL);
	LIST_REMOVE(tracee, link);
	return 0;
}

/**
 * Allocate a new entry for the tracee @pid.
 */
static struct tracee *new_tracee(pid_t pid)
{
	struct tracee *tracee;

	tracee = talloc_zero(NULL, struct tracee);
	if (tracee == NULL)
		notice(ERROR, INTERNAL, "talloc_zero() failed");

	talloc_set_destructor(tracee, remove_tracee);

	tracee->pid = pid;
	LIST_INSERT_HEAD(&tracees, tracee, link);

	return tracee;
}

/**
 * Return the entry related to the tracee @pid.  If no entry were
 * found, a new one is created if @create is true, otherwise NULL is
 * returned.
 */
struct tracee *get_tracee(pid_t pid, bool create)
{
	struct tracee *tracee;

	LIST_FOREACH(tracee, &tracees, link)
		if (tracee->pid == pid)
			return tracee;

	return (create ? new_tracee(pid) : NULL);
}

/**
 * Make the @child tracee inherit filesystem information from the
 * @parent tracee.  Depending on the @parent->clone_flags, some
 * information are copied or shared.
 */
void inherit_fs_info(struct tracee *child, struct tracee *parent)
{
	assert(child->exe  == NULL);
	// assert(child->root == NULL);
	// assert(child->cwd  == NULL);

	/* The first tracee is started by PRoot and does nothing but a
	 * call to execve(2), thus child->exe will be automatically
	 * updated later.  */
	if (parent == NULL) {
		child->exe = talloc_strdup(child, "<dummy>");
		//child->root = talloc_strdup(config.guest_rootfs);
		//child->cwd  = talloc_strdup(config.initial_cwd);
		return;
	}

	assert(parent->exe  != NULL);
	// assert(parent->root != NULL);
	// assert(parent->cwd  != NULL);

	/* The path to the executable is unshared only once the child
	 * process does a call to execve(2).  */
	child->exe = talloc_reference(child, parent->exe);

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
		child->root = talloc_reference(child, parent->root);
		child->cwd  = talloc_reference(child, parent->cwd);
	}
	else {
		/* File-system information is copied.  */
		child->root = talloc_strdup(child, parent->root);
		child->cwd  = talloc_strdup(child, parent->cwd);
	}
#endif

	return;
}
