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

#include <sys/ptrace.h> /* PTRACE_*,  */
#include <sys/wait.h>   /* __WALL,  */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <stdbool.h>    /* bool, true, false, */

#include "ptrace/wait.h"
#include "ptrace/event.h"
#include "ptrace/ptrace.h"
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "notice.h"

/**
 * Translate the wait syscall made by @ptracer into a "void" syscall
 * if the expected pid is one of its ptracees, in order to emulate the
 * ptrace mechanism within PRoot.  This function returns -errno if an
 * error occured (unsupported request), otherwise 0.
 */
int translate_wait_enter(Tracee *ptracer)
{
	static bool warned = false;
	Tracee *ptracee;
	word_t options;
	word_t pid;

	pid = peek_reg(ptracer, ORIGINAL, SYSARG_1);

	/* Don't emulate the ptrace mechanism if its not a a ptracee.
	 * However, this syscall will be canceled later if a ptracee
	 * is attached to this ptracer.  */
	ptracee = get_ptracee(ptracer, pid, false);
	if (ptracee == NULL) {
		PTRACER.waits_in_kernel = true;
		return 0;
	}

	/* Only the __WALL option is supported so far.  */
	options = peek_reg(ptracer, ORIGINAL, SYSARG_3);
	if (options != __WALL && !warned) {
		notice(ptracer, INTERNAL, WARNING, "only __WALL option is supported yet");
		warned = true;
	}

	/* This syscall is canceled at the enter stage in order to be
	 * handled at the exit stage.  */
	set_sysnum(ptracer, PR_void);
	PTRACER.waits_in_proot = true;

	return 0;
}

/**
 * Emulate the wait* syscall made by @ptracer if it was in the context
 * of the ptrace mechanism. This function returns -errno if an error
 * occured, otherwise the pid of the expected tracee.
 */
int translate_wait_exit(Tracee *ptracer)
{
	Tracee *ptracee;
	word_t pid;

	assert(PTRACER.waits_in_proot);
	PTRACER.waits_in_proot = false;

	pid = peek_reg(ptracer, ORIGINAL, SYSARG_1);

	/* Is the expected pracee waiting for this ptracer?  */
	ptracee = get_ptracee(ptracer, pid, true);
	if (ptracee == NULL) {
		/* Is there still living ptracees?  */
		if (PTRACER.nb_tracees == 0)
			return -ECHILD;

		/* Otherwise put this ptracer in the "waiting for
		 * ptracee" state.  */
		PTRACER.wait_pid = pid;
		return 0;
	}

	/* Update ptracer's registers & memory according to this
	 * ptracee state.  */
	PTRACER.wait_pid = 0;
	(void) notify_ptracer(ptracee);

	return ptracee->pid;
}
