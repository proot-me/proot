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
#include <sys/queue.h>  /* LIST_*,  */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <signal.h>     /* siginfo_t, TRAP_*, */
#include <unistd.h>     /* getpid(2), */
#include <talloc.h>     /* talloc*, */

#include "ptrace/wait.h"
#include "ptrace/ptrace.h"
#include "execve/auxv.h"
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "cli/notice.h"

#include "attribute.h"

static const char *stringify_event(int event) UNUSED;
static const char *stringify_event(int event)
{
	if (WIFEXITED(event))
		return "exited";
	else if (WIFSIGNALED(event))
		return "signaled";
	else if (WIFCONTINUED(event))
		return "continued";
	else if (WIFSTOPPED(event)) {
		switch ((event & 0xfff00) >> 8) {
		case SIGTRAP:
			return "stopped: SIGTRAP";
		case SIGTRAP | 0x80:
			return "stopped: SIGTRAP: 0x80";
		case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_VFORK";
		case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_FORK";
		case SIGTRAP | PTRACE_EVENT_VFORK_DONE  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_FORK_DONE";
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

/**
 * Translate the wait syscall made by @ptracer into a "void" syscall
 * if the expected pid is one of its ptracees, in order to emulate the
 * ptrace mechanism within PRoot.  This function returns -errno if an
 * error occured (unsupported request), otherwise 0.
 */
int translate_wait_enter(Tracee *ptracer)
{
	Tracee *ptracee;
	pid_t pid;

	PTRACER.waits_in = WAITS_IN_KERNEL;

	/* Don't emulate the ptrace mechanism if it's not a ptracer.  */
	if (PTRACER.nb_ptracees == 0)
		return 0;

	/* Don't emulate the ptrace mechanism if the requested pid is
	 * not a ptracee.  */
	pid = (pid_t) peek_reg(ptracer, ORIGINAL, SYSARG_1);
	if (pid != -1) {
		ptracee = get_tracee(ptracer, pid, false);
		if (ptracee == NULL || PTRACEE.ptracer != ptracer)
			return 0;
	}

	/* This syscall is canceled at the enter stage in order to be
	 * handled at the exit stage.  */
	set_sysnum(ptracer, PR_void);
	PTRACER.waits_in = WAITS_IN_PROOT;

	return 0;
}

/**
 * Update pid & wait status of @ptracer's wait(2) for the given
 * @ptracee.
 */
static int update_wait_status(Tracee *ptracer, Tracee *ptracee)
{
	const int event = PTRACEE.event4.ptracer.value;
	word_t address;

	address = peek_reg(ptracer, ORIGINAL, SYSARG_2);
	if (address != 0) {
		poke_int32(ptracer, address, event);
		if (errno != 0)
			return -errno;
	}

	PTRACEE.event4.ptracer.pending = false;

	/* Under PRoot, the kernel will report its termination once
	 * again to its parent since "ptracer != parent" from kernel's
	 * point-of-view.  PRoot has to mask this second notification
	 * not to make the parent/ptracer confused.  */
	if (   (WIFEXITED(event) || WIFSIGNALED(event))
	    && is_direct_ptracee(ptracer, ptracee->pid)) {
		set_exited_direct_ptracee(ptracer, ptracee->pid);
	}

	return ptracee->pid;
}


/**
 * Emulate the wait* syscall made by @ptracer if it was in the context
 * of the ptrace mechanism. This function returns -errno if an error
 * occured, otherwise the pid of the expected tracee.
 */
int translate_wait_exit(Tracee *ptracer)
{
	Tracee *ptracee;
	word_t options;
	int status;
	pid_t pid;

	assert(PTRACER.waits_in == WAITS_IN_PROOT);
	PTRACER.waits_in = DOESNT_WAIT;

	pid = (pid_t) peek_reg(ptracer, ORIGINAL, SYSARG_1);
	options = peek_reg(ptracer, ORIGINAL, SYSARG_3);

	/* Is there such a stopped ptracee with an event not yet
	 * passed to its ptracer?  */
	ptracee = get_stopped_ptracee(ptracer, pid, true, options);
	if (ptracee == NULL) {
		/* Is there still living ptracees?  */
		if (PTRACER.nb_ptracees == 0)
			return -ECHILD;

		/* Non blocking wait(2) ?  */
		if ((options & WNOHANG) != 0) {
			/* if WNOHANG was specified and one or more
			 * child(ren) specified by pid exist, but have
			 * not yet changed state, then 0 is returned.
			 * On error, -1 is returned.
			 *
			 * -- man 2 waitpid  */
			return (has_ptracees(ptracer, pid, options) ? 0 : -ECHILD);
		}

		/* Otherwise put this ptracer in the "waiting for
		 * ptracee" state, it will be woken up in
		 * handle_ptracee_event() later.  */
		PTRACER.wait_pid = pid;
		PTRACER.wait_options = options;
		return 0;
	}

	status = update_wait_status(ptracer, ptracee);
	if (status < 0)
		return status;

	pid = ptracee->pid;

	/* Zombies can rest in peace once the ptracer is notified.  */
	if (PTRACEE.is_zombie)
		TALLOC_FREE(ptracee);

	return pid;
}

/**
 * For the given @ptracee, pass its current @event to its ptracer if
 * this latter is waiting for it, otherwise put the @ptracee in the
 * "waiting for ptracer" state.  This function returns whether
 * @ptracee shall be kept in the stop state or not.
 */
bool handle_ptracee_event(Tracee *ptracee, int event)
{
	bool handled_by_proot_first = false;
	Tracee *ptracer = PTRACEE.ptracer;
	bool keep_stopped;

	assert(ptracer != NULL);

	/* Remember what the event initially was, this will be
	 * required by PRoot to handle this event later.  */
	PTRACEE.event4.proot.value   = event;
	PTRACEE.event4.proot.pending = true;

	/* By default, this ptracee should be kept stopped until its
	 * ptracer restarts it.  */
	keep_stopped = true;

	/* Not all events are expected for this ptracee.  */
	if (WIFSTOPPED(event)) {
		switch ((event & 0xfff00) >> 8) {
		case SIGTRAP | 0x80:
			/* Fix ELF auxiliary vectors.  So far, this is
			 * only required to make GDB work correctly
			 * under PRoot.  */
			if (IS_IN_SYSEXIT2(ptracee, PR_execve)
			    && fetch_regs(ptracee) >= 0
			    && (int) peek_reg(ptracee, CURRENT, SYSARG_RESULT) >= 0) {
				fix_elf_aux_vectors(ptracee);
			}

			/* If the PTRACE_O_TRACEEXEC option is *not*
			 * in effect for the execing tracee, the
			 * kernel delivers an extra SIGTRAP to the
			 * tracee after execve(2) *returns*.  This is
			 * an ordinary signal (similar to one which
			 * can be generated by "kill -TRAP"), not a
			 * special kind of ptrace-stop.  Employing
			 * PTRACE_GETSIGINFO for this signal returns
			 * si_code set to 0 (SI_USER).  This signal
			 * may be blocked by signal mask, and thus may
			 * be delivered (much) later.
			 *
			 * -- man 2 ptrace
			 *
			 * Note about the is_loader/PR_close trick: it
			 * is required to make GDB usable under PRoot.
			 * GDB assumes the program is loaded in memory
			 * once execve(2) has completed, however this
			 * is not the case under PRoot since it
			 * replaces the executed programs with a
			 * loader (the ELF interpreter for now).  As a
			 * consequence, only the loader is in memory
			 * and GDB will fail to set breakpoints on the
			 * program.  A workaround is to delay the
			 * execve(2) notification until the ELF
			 * interpreter has loaded the program in
			 * memory, that is, until the first PR_close
			 * has completed.  */
			if (IS_IN_SYSEXIT2(ptracee, (PTRACEE.is_load_pending
									? PR_close
									: PR_execve))
			    && (PTRACEE.options & PTRACE_O_TRACEEXEC) == 0
			    && fetch_regs(ptracee) >= 0
			    && (int) peek_reg(ptracee, CURRENT, SYSARG_RESULT) >= 0) {
				kill(ptracee->pid, SIGTRAP);
				PTRACEE.is_load_pending = false;
			}

			if (PTRACEE.ignore_syscall)
				return false;

			if ((PTRACEE.options & PTRACE_O_TRACESYSGOOD) == 0)
				event &= ~(0x80 << 8);

			handled_by_proot_first = IS_IN_SYSEXIT(ptracee);
			break;

#define PTRACE_EVENT_VFORKDONE PTRACE_EVENT_VFORK_DONE
#define CASE_FILTER_EVENT(name) \
		case SIGTRAP | PTRACE_EVENT_ ##name << 8:			\
			if ((PTRACEE.options & PTRACE_O_TRACE ##name) == 0)	\
				return false;					\
			PTRACEE.tracing_started = true;				\
			handled_by_proot_first = true;				\
			break;

		CASE_FILTER_EVENT(FORK);
		CASE_FILTER_EVENT(VFORK);
		CASE_FILTER_EVENT(VFORKDONE);
		CASE_FILTER_EVENT(CLONE);
		CASE_FILTER_EVENT(EXIT);
		CASE_FILTER_EVENT(EXEC);

			/* Never reached.  */
			assert(0);

		default:
			PTRACEE.tracing_started = true;
			break;
		}
	}
	/* In these cases, the ptracee isn't really alive anymore.  To
	 * ensure it will not be in limbo, PRoot restarts it whether
	 * its ptracer is waiting for it or not.  */
	else if (WIFEXITED(event) || WIFSIGNALED(event)) {
		PTRACEE.tracing_started = true;
		keep_stopped = false;
	}

	/* A process is not traced right from the TRACEME request; it
	 * is traced from the first received signal, whether it was
	 * raised by the process itself (implicitly or explicitly), or
	 * it was induced by a PTRACE_EVENT_*.  */
	if (!PTRACEE.tracing_started)
		return false;

	/* Under some circumstances, the event must be handled by
	 * PRoot first.  */
	if (handled_by_proot_first) {
		int signal;
		signal = handle_tracee_event(ptracee, PTRACEE.event4.proot.value);
		PTRACEE.event4.proot.value = signal;

		/* The computed signal is always 0 since we can come
		 * in this block only on sysexit and special events
		 * (as for now).  */
		assert(signal == 0);
	}

	/* Remember what the new event is, this will be required by
	   the ptracer in translate_ptrace_exit() in order to restart
	   this ptracee.  */
	PTRACEE.event4.ptracer.value   = event;
	PTRACEE.event4.ptracer.pending = true;

	/* Notify asynchronously the ptracer about this event, as the
	 * kernel does.  */
	kill(ptracer->pid, SIGCHLD);

	/* Note: wait_pid is set in translate_wait_exit() if no
	 * ptracee event was pending when the ptracer started to
	 * wait.  */
	if (   (PTRACER.wait_pid == -1 || PTRACER.wait_pid == ptracee->pid)
	    && EXPECTED_WAIT_CLONE(PTRACER.wait_options, ptracee)) {
		bool restarted;
		int status;

		status = update_wait_status(ptracer, ptracee);
		poke_reg(ptracer, SYSARG_RESULT, (word_t) status);

		/* Write ptracer's register cache back.  */
		(void) push_regs(ptracer);

		/* Restart the ptracer.  */
		PTRACER.wait_pid = 0;
		restarted = restart_tracee(ptracer, 0);
		if (!restarted)
			keep_stopped = false;

		return keep_stopped;
	}

	return keep_stopped;
}

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
