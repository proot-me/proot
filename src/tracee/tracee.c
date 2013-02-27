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

#include <sys/types.h>  /* pid_t, size_t, */
#include <stdlib.h>     /* NULL, */
#include <assert.h>     /* assert(3), */
#include <string.h>     /* bzero(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <sys/queue.h>  /* LIST_*,  */
#include <talloc.h>     /* talloc_*, */
#include <signal.h>     /* kill(2), SIGKILL, */
#include <sys/ptrace.h>  /* ptrace(2), PTRACE_*, */
#include <errno.h>      /* E*, */

#include "tracee/tracee.h"
#include "extension/extension.h"
#include "notice.h"

#include "compat.h"

typedef LIST_HEAD(tracees, tracee) Tracees;
static Tracees tracees;

/**
 * Remove @tracee from the list of tracees.
 */
static int remove_tracee(Tracee *tracee)
{
	LIST_REMOVE(tracee, link);
	return 0;
}

/**
 * Allocate a new entry for the tracee @pid.
 */
static Tracee *new_tracee(pid_t pid)
{
	Tracee *tracee;

	tracee = talloc_zero(NULL, Tracee);
	if (tracee == NULL)
		return NULL;
	talloc_set_destructor(tracee, remove_tracee);

	/* Allocate a memory collector.  */
	tracee->ctx = talloc_new(tracee);
	if (tracee->ctx == NULL)
		goto no_mem;

	/* By default new tracees have an empty file-system
	 * name-space.  */
	tracee->fs = talloc_zero(tracee, FileSystemNameSpace);
	if (tracee->fs == NULL)
		goto no_mem;

	tracee->pid = pid;
	LIST_INSERT_HEAD(&tracees, tracee, link);

	return tracee;

no_mem:
	TALLOC_FREE(tracee);
	return NULL;
}

/**
 * Return the entry related to the tracee @pid.  If no entry were
 * found, a new one is created if @create is true, otherwise NULL is
 * returned.
 */
Tracee *get_tracee(const Tracee *current_tracee, pid_t pid, bool create)
{
	Tracee *tracee;

	/* Don't reset the memory collector if the searched tracee is
	 * the current one: there's likely pointers to the
	 * sub-allocated data in the caller.  */
	if (current_tracee != NULL && current_tracee->pid == pid)
		return (Tracee *)current_tracee;

	LIST_FOREACH(tracee, &tracees, link)
		if (tracee->pid == pid) {
			/* Flush then allocate a new memory collector.  */
			TALLOC_FREE(tracee->ctx);
			tracee->ctx = talloc_new(tracee);

			return tracee;
		}

	return (create ? new_tracee(pid) : NULL);
}

/**
 * Make the @child tracee inherit from the @parent tracee.  Depending
 * on @shared_fs, some information are copied or shared.  This
 * function returns -errno if an error occured, otherwise 0.
 */
int inherit_config(Tracee *child, Tracee *parent, bool shared_fs)
{
	assert(parent != NULL);

	assert(child->exe == NULL && parent->exe != NULL);
	assert(child->cmdline == NULL && parent->cmdline != NULL);
	assert(child->fs->cwd == NULL && parent->fs->cwd != NULL);
	assert(child->fs->bindings.pending == NULL && parent->fs->bindings.pending == NULL);
	assert(child->fs->bindings.guest == NULL   && parent->fs->bindings.guest != NULL);
	assert(child->fs->bindings.host == NULL    && parent->fs->bindings.host != NULL);
	assert(child->qemu == NULL);
	assert(child->glue == NULL);

	child->verbose = parent->verbose;

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
	TALLOC_FREE(child->fs);
	if (shared_fs) {
		/* File-system name-space is shared.  */
		child->fs = talloc_reference(child, parent->fs);
	}
	else {
		/* File-system name-space is copied.  */
		child->fs = talloc_zero(child, FileSystemNameSpace);
		if (child->fs == NULL)
			return -ENOMEM;

		child->fs->cwd = talloc_strdup(child->fs, parent->fs->cwd);
		if (child->fs->cwd == NULL)
			return -ENOMEM;
		talloc_set_name_const(child->fs->cwd, "$cwd");

		/* Bindings are shared across file-system name-spaces since a
		 * "mount --bind" made by a process affects all other processes
		 * under Linux.  Actually they are copied when a sub
		 * reconfiguration occured (nested proot or chroot(2)).  */
		child->fs->bindings.guest = talloc_reference(child->fs, parent->fs->bindings.guest);
		child->fs->bindings.host  = talloc_reference(child->fs, parent->fs->bindings.host);
	}

	/* The path to the executable and the command-line are unshared only
	 * once the child process does a call to execve(2).  */
	child->exe = talloc_reference(child, parent->exe);
	child->cmdline = talloc_reference(child, parent->cmdline);

	child->qemu_pie_workaround = parent->qemu_pie_workaround;
	child->qemu = talloc_reference(child, parent->qemu);
	child->glue = talloc_reference(child, parent->glue);

	inherit_extensions(child, parent, false);

	/* Restart the child tracee if it was already alive but
	 * stopped until that moment.  */
	if (child->sigstop == SIGSTOP_PENDING) {
		int status;

		child->sigstop = SIGSTOP_ALLOWED;
		status = ptrace(PTRACE_SYSCALL, child->pid, NULL, 0);
		if (status < 0)
			TALLOC_FREE(child);
	}

	return 0;
}

/**
 * Swap configuration (pointers and parentality) between @tracee1 and @tracee2.
 */
int swap_config(Tracee *tracee1, Tracee *tracee2)
{
	Tracee *tmp;

	tmp = talloc_zero(tracee1->ctx, Tracee);
	if (tmp == NULL)
		return -ENOMEM;

#if defined(TALLOC_VERSION_MAJOR) && TALLOC_VERSION_MAJOR >= 2
	void reparent_config(Tracee *new_parent, Tracee *old_parent) {
		new_parent->verbose = old_parent->verbose;
		new_parent->qemu_pie_workaround = old_parent->qemu_pie_workaround;

		#define REPARENT(field) do {						\
			talloc_reparent(old_parent, new_parent, old_parent->field);	\
			new_parent->field = old_parent->field;				\
		} while(0);

		REPARENT(fs);
		REPARENT(exe);
		REPARENT(cmdline);
		REPARENT(qemu);
		REPARENT(glue);
		REPARENT(extensions);
	}

	reparent_config(tmp,     tracee1);
	reparent_config(tracee1, tracee2);
	reparent_config(tracee2, tmp);

	return 0;
#else
	return -ENOSYS;
#endif
}

/* Send the KILL signal to all tracees.  */
void kill_all_tracees()
{
	Tracee *tracee;

	LIST_FOREACH(tracee, &tracees, link)
		kill(tracee->pid, SIGKILL);
}
