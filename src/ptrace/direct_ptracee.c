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

#include <sys/queue.h>  /* LIST_*,  */
#include <unistd.h>     /* getpid(2), */
#include <talloc.h>     /* talloc*, */
#include <errno.h>     /* E*, */

#include "ptrace/direct_ptracee.h"
#include "ptrace/ptrace.h"
#include "cli/notice.h"

struct direct_ptracee {
	pid_t pid;
	bool has_exited;
	LIST_ENTRY(direct_ptracee) link;
};

LIST_HEAD(direct_ptracees, direct_ptracee);

/**
 * Add @ptracee_pid to @ptracer's list of direct ptracees.  This
 * function returns -errno if an error occured, otherwise 0.
 */
int add_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	if (is_direct_ptracee(ptracer, ptracee_pid))
		notice(ptracer, WARNING, INTERNAL,
			"%s: pid %d is already declared as direct ptracee.",
			__FUNCTION__, ptracee_pid);

	if (PTRACER.direct_ptracees == NULL) {
		PTRACER.direct_ptracees = talloc_zero(ptracer, struct direct_ptracees);
		if (PTRACER.direct_ptracees == NULL)
			return -ENOMEM;
	}

	direct_ptracee = talloc_zero(PTRACER.direct_ptracees, struct direct_ptracee);
	if (direct_ptracee == NULL)
		return -ENOMEM;

	direct_ptracee->pid = ptracee_pid;

	LIST_INSERT_HEAD(PTRACER.direct_ptracees, direct_ptracee, link);

	return 0;
}

/**
 * Return the entry from @ptracer's list of direct ptracees for the
 * given @ptracee_pid, or NULL if it does not exist.
 */
static struct direct_ptracee *get_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	if (PTRACER.direct_ptracees == NULL)
		return NULL;

	LIST_FOREACH(direct_ptracee, PTRACER.direct_ptracees, link) {
		if (direct_ptracee->pid == ptracee_pid)
			return direct_ptracee;
	}

	return NULL;
}

/**
 * Check whether @ptracee_pid belongs to @ptracer's list of direct
 * ptracees.
 */
bool is_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	direct_ptracee = get_direct_ptracee(ptracer, ptracee_pid);
	if (direct_ptracee == NULL)
		return false;

	if (direct_ptracee->has_exited)
		notice(ptracer, WARNING, INTERNAL,
			"%s: direct ptracee %d is expected to not be already exited.",
			__FUNCTION__, ptracee_pid);

	return true;
}

/**
 * Declare @ptracee_pid -- from @ptracer's list of direct ptracees --
 * has exited.
 */
void set_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	direct_ptracee = get_direct_ptracee(ptracer, ptracee_pid);
	if (direct_ptracee == NULL) {
		notice(ptracer, WARNING, INTERNAL,
			"%s: pid %d is expected to be declared as direct ptracee.",
			__FUNCTION__, ptracee_pid);
		return;
	}

	if (direct_ptracee->has_exited)
		notice(ptracer, WARNING, INTERNAL,
			"%s: direct ptracee %d is expected to not be already exited.",
			__FUNCTION__, ptracee_pid);

	direct_ptracee->has_exited = true;
}

/**
 * Check whether @ptracee_pid belongs to @ptracer's list of direct
 * ptracees and is declared as exited.
 */
bool is_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	direct_ptracee = get_direct_ptracee(ptracer, ptracee_pid);
	if (direct_ptracee == NULL)
		return false;

	if (!direct_ptracee->has_exited)
		return false;

	return true;
}

/**
 * Remove @ptracee_pid from @ptracer's list of direct ptracees.
 */
void remove_exited_direct_ptracee(Tracee *ptracer, pid_t ptracee_pid)
{
	struct direct_ptracee *direct_ptracee;

	direct_ptracee = get_direct_ptracee(ptracer, ptracee_pid);
	if (direct_ptracee == NULL) {
		notice(ptracer, WARNING, INTERNAL,
			"%s: pid %d is expected to be declared as direct ptracee.",
			__FUNCTION__, ptracee_pid);
		return;
	}

	LIST_REMOVE(direct_ptracee, link);

	/* No more direct ptracees?  */
	if (LIST_EMPTY(PTRACER.direct_ptracees))
		TALLOC_FREE(PTRACER.direct_ptracees);
}
