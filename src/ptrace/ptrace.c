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
#include <sys/wait.h>   /* WIFSTOPPED,  */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <stdbool.h>    /* bool, true, false, */

#include "ptrace/ptrace.h"
#include "ptrace/event.h"
#include "tracee/tracee.h"
#include "syscall/sysnum.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "tracee/event.h"
#include "notice.h"

static const char *stringify_ptrace(enum __ptrace_request request)
{
#define CASE_STR(a) case a: return #a; break;
	switch (request) {
	CASE_STR(PTRACE_TRACEME)	CASE_STR(PTRACE_PEEKTEXT)	CASE_STR(PTRACE_PEEKDATA)
	CASE_STR(PTRACE_PEEKUSER)	CASE_STR(PTRACE_POKETEXT)	CASE_STR(PTRACE_POKEDATA)
	CASE_STR(PTRACE_POKEUSER)	CASE_STR(PTRACE_CONT)		CASE_STR(PTRACE_KILL)
	CASE_STR(PTRACE_SINGLESTEP)	CASE_STR(PTRACE_GETREGS)	CASE_STR(PTRACE_SETREGS)
	CASE_STR(PTRACE_GETFPREGS)	CASE_STR(PTRACE_SETFPREGS)	CASE_STR(PTRACE_ATTACH)
	CASE_STR(PTRACE_DETACH)		CASE_STR(PTRACE_GETFPXREGS)	CASE_STR(PTRACE_SETFPXREGS)
	CASE_STR(PTRACE_SYSCALL)	CASE_STR(PTRACE_SETOPTIONS)	CASE_STR(PTRACE_GETEVENTMSG)
	CASE_STR(PTRACE_GETSIGINFO)	CASE_STR(PTRACE_SETSIGINFO)	CASE_STR(PTRACE_GETREGSET)
	CASE_STR(PTRACE_SETREGSET)	CASE_STR(PTRACE_SEIZE)		CASE_STR(PTRACE_INTERRUPT)
	CASE_STR(PTRACE_LISTEN) default: return "PTRACE_???"; }
}

/**
 * Translate the ptrace syscall made by @tracee into a "void" syscall
 * in order to emulate the ptrace mechanism within PRoot.  This
 * function returns -errno if an error occured (unsupported request),
 * otherwise 0.
 */
int translate_ptrace_enter(Tracee *tracee)
{
	word_t request;

	/* The ptrace syscall have to be emulated since it can't be nested.  */
	set_sysnum(tracee, PR_void);

	request = peek_reg(tracee, CURRENT, SYSARG_1);
	switch (request) {
	case PTRACE_ATTACH:
	case PTRACE_KILL:
		/* Asynchronous requests.  */
		notice(tracee, WARNING, INTERNAL, "ptrace request '%s' not supported yet",
			stringify_ptrace(request));
		return -ENOTSUP;

	default:
		/* Other requests are handled at the exit stage.  */
		break;
	}

	return 0;
}

/**
 * Emulate the ptrace syscall made by @tracee.  This function returns
 * -errno if an error occured (unsupported request), otherwise 0.
 */
int translate_ptrace_exit(Tracee *tracee)
{
	word_t request, pid, address, data, result;
	Tracee *ptracee, *ptracer;
	int status;

	/* Read ptrace parameters.  */
	request = peek_reg(tracee, ORIGINAL, SYSARG_1);
	pid     = peek_reg(tracee, ORIGINAL, SYSARG_2);
	address = peek_reg(tracee, ORIGINAL, SYSARG_3);
	data    = peek_reg(tracee, ORIGINAL, SYSARG_4);

	/* The TRACEME request is the only one used by a tracee.  */
	if (request == PTRACE_TRACEME) {
		ptracer = tracee->parent;
		ptracee = tracee;

		/* The emulated ptrace in PRoot has the same
		 * limitation as the real ptrace in the Linux kernel:
		 * only one tracer per process.  */
		if (PTRACEE.tracer != NULL || ptracee == ptracer)
			return -EPERM;

		PTRACEE.tracer = ptracer;
		PTRACER.nb_tracees++;

		return 0;
	}

	/* Here, the tracee is a ptracer.  Also, the requested ptracee
	 * has to be in the "waiting for ptracer" state.  */
	ptracer = tracee;
	ptracee = get_ptracee(ptracer, pid, true);
	if (ptracee == NULL)
		return -ESRCH;

	/* Sanity checks.  */
	if (PTRACEE.tracer != ptracer || pid == -1)
		return -ESRCH;

	switch (request) {
	case PTRACE_SYSCALL:
		/* Restart the stopped ptracee, and stop at the next
		 * syscall.  */
		PTRACEE.ignore_syscall = false;
		status = 0;
		break;

	case PTRACE_CONT:
		/* Restart the stopped ptracee, but don't stop at the
		 * next syscall.  */
		PTRACEE.ignore_syscall = true;
		status = 0;
		break;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
	case PTRACE_PEEKUSER:
		/* Fulfill these requests on behalve of the emulated
		 * ptracer.  */
		errno = 0;
		result = (word_t) ptrace(request, pid, address, NULL);
		if (errno != 0) {
			status = -errno;
			goto wake_tracee;
		}

		poke_mem(ptracer, data, result);
		if (errno != 0) {
			status = -errno;
			goto wake_tracee;
		}
		return 0;

	default:
		notice(ptracer, WARNING, INTERNAL, "ptrace request '%s' not supported yet",
			stringify_ptrace(request));
		status = -ENOTSUP;
		goto wake_tracee;
	}

wake_tracee:
	/* Now, the initial tracee's event can be handled.  */
	handle_tracee_event(ptracee, PTRACEE.wait_status);

	return status;
}
