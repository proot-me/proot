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
#include <stdbool.h>    /* bool, true, false, */
#include <assert.h>     /* assert(3), */
#include <signal.h>     /* SIGSTOP, SIGTRAP, */

#include "ptrace/event.h"
#include "ptrace/ptrace.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "notice.h"

#if 0
static const char *stringify_status(int wait_status)
{
	if (WIFEXITED(wait_status))
		return "exited";
	else if (WIFSIGNALED(wait_status))
		return "signaled";
	else if (WIFCONTINUED(wait_status))
		return "continued";
	else if (WIFSTOPPED(wait_status)) {
		switch ((wait_status & 0xfff00) >> 8) {
		case SIGTRAP:
			return "stopped: SIGTRAP";
		case SIGTRAP | 0x80:
			return "stopped: SIGTRAP: 0x80";
		case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_VFORK";
		case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_FORK";
		case SIGTRAP | PTRACE_EVENT_CLONE << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_CLONE";
		case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_EXEC";
		case SIGTRAP | PTRACE_EVENT_EXIT  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_EXIT";
		case SIGSTOP:
			return "stopped: SIGSTOP";
		default:
			return "stopped: unknown";
		}
	}
	return "unknown";
}
#endif

/**
 * Update the ptracer's registers and memory according to its @ptracee
 * state, then restart this former if required.  This function returns
 * whether the ptracer when restarted or not.
 */
bool notify_ptracer(Tracee *ptracee)
{
	Tracee *ptracer = PTRACEE.tracer;
	word_t wait_status;
	word_t address;

	assert(PTRACEE.waits_tracer);

	/* Not all events are expected for this ptracee.  */
	wait_status = PTRACEE.wait_status;
	if (WIFSTOPPED(wait_status)) {
		switch ((wait_status & 0xfff00) >> 8) {
		case SIGTRAP | 0x80:
			if (PTRACEE.ignore_syscall)
				return false;

			if ((PTRACEE.options & PTRACE_O_TRACESYSGOOD) == 0)
				wait_status &= ~(0x80 << 8);
			break;

#define CASE_FILTER_EVENT(name)							\
		case SIGTRAP | PTRACE_EVENT_ ##name << 8:			\
			if ((PTRACEE.options & PTRACE_O_TRACE ##name) == 0)	\
				return false;					\
			break;

		CASE_FILTER_EVENT(FORK);
		CASE_FILTER_EVENT(VFORK);
		CASE_FILTER_EVENT(CLONE);
		CASE_FILTER_EVENT(EXEC);
		CASE_FILTER_EVENT(EXIT);

		default:
			break;
		}
	}

	/* Update the return value of ptracer's wait(2).  */
	poke_reg(ptracer, SYSARG_RESULT, ptracee->pid);

	/* Update the child status of ptracer's wait(2), if any.  */
	address = peek_reg(ptracer, ORIGINAL, SYSARG_2);
	if (address != 0)
		poke_mem(ptracer, address, wait_status);

	/* Nothing else to do if the ptracer is running, that is, it
	 * isn't in the "waiting for ptracee" state (typically when
	 * called from translate_wait_exit).  */
	if (PTRACER.wait_pid == 0)
		return false;

	/* The pending wait* syscall is done: restart the ptracer.  */
	PTRACER.wait_pid = 0;
	restart_tracee(ptracer, 0);

	return true;
}

/**
 * For the given @ptracee, notify its ptracer that this former has
 * triggered an event, if this latter is in the "waiting for ptracee"
 * state.  This function returns true if @ptracee should be put in the
 * "wait for ptracer" state, otherwise false.
 */
bool handle_ptracee_event(Tracee *ptracee, int ptracee_status)
{
	Tracee *ptracer = PTRACEE.tracer;
	assert(ptracer != NULL);

	PTRACEE.waits_tracer = true;
	PTRACEE.wait_status  = ptracee_status;

	/* Put this ptracee in the "waiting for ptracer" state.  */
	if (PTRACER.wait_pid != ptracee->pid && PTRACER.wait_pid != -1)
		return true;

	return notify_ptracer(ptracee);
}
