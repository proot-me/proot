/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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
 */

#define _GNU_SOURCE      /* O_NOFOLLOW in fcntl.h, */
#include <fcntl.h>       /* AT_FDCWD, */
#include <sys/types.h>   /* pid_t, */
#include <assert.h>      /* assert(3), */
#include <limits.h>      /* PATH_MAX, */
#include <stddef.h>      /* intptr_t, offsetof(3), */
#include <errno.h>       /* errno(3), */
#include <sys/ptrace.h>  /* ptrace(2), PTRACE_*, */
#include <sys/user.h>    /* struct user*, */
#include <stdlib.h>      /* exit(3), */
#include <string.h>      /* strlen(3), */
#include <sys/utsname.h> /* struct utsname, */

#include "syscall/syscall.h"
#include "arch.h"
#include "tracee/mem.h"
#include "tracee/info.h"
#include "path/path.h"
#include "execve/execve.h"
#include "notice.h"
#include "tracee/ureg.h"
#include "config.h"

#include "compat.h"

/**
 * Copy in @path a C string (PATH_MAX bytes max.) from the @tracee's
 * memory address space pointed to by the @sysarg argument of the
 * current syscall.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes put into the @path.
 */
int get_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum sysarg sysarg)
{
	int size;
	word_t src;

	src = peek_ureg(tracee, sysarg);
	if (errno != 0)
		return -errno;

	/* Check if the parameter is not NULL. Technically we should
	 * not return an -EFAULT for this special value since it is
	 * allowed for some syscall, utimensat(2) for instance. */
	if (src == 0) {
		path[0] = '\0';
		return 0;
	}

	/* Get the path from the tracee's memory space. */
	size = get_tracee_string(tracee, path, src, PATH_MAX);
	if (size < 0)
		return size;
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	path[size] = '\0';
	return size;
}

/**
 * Copy @path in the @tracee's memory address space pointed to by
 * the @sysarg argument of the current syscall.  This function returns
 * -errno if an error occured, otherwise it returns the size in bytes
 * "allocated" into the stack.
 */
int set_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum sysarg sysarg)
{
	word_t tracee_ptr;
	int status;
	int size;

	/* Allocate space into the tracee's stack to host the new path. */
	size = strlen(path) + 1;
	tracee_ptr = resize_tracee_stack(tracee, size);
	if (tracee_ptr == 0)
		return -EFAULT;

	/* Copy the new path into the previously allocated space. */
	status = copy_to_tracee(tracee, tracee_ptr, path, size);
	if (status < 0) {
		(void) resize_tracee_stack(tracee, -size);
		return status;
	}

	/* Make this argument point to the new path. */
	status = poke_ureg(tracee, sysarg, tracee_ptr);
	if (status < 0) {
		(void) resize_tracee_stack(tracee, -size);
		return status;
	}

	return size;
}

/**
 * Translate @path and put the result in the @tracee's memory address
 * space pointed to by the @sysarg argument of the current
 * syscall. See the documentation of translate_path() about the
 * meaning of @deref_final. This function returns -errno if an error
 * occured, otherwise it returns the size in bytes "allocated" into
 * the stack.
 */
static int translate_path2(struct tracee_info *tracee, int dir_fd, char path[PATH_MAX], enum sysarg sysarg, int deref_final)
{
	char new_path[PATH_MAX];
	int status;

	/* Special case where the argument was NULL. */
	if (path[0] == '\0')
		return 0;

	/* Translate the original path. */
	status = translate_path(tracee, new_path, dir_fd, path, deref_final);
	if (status < 0)
		return status;

	return set_sysarg_path(tracee, new_path, sysarg);
}

/**
 * A helper, see the comment of the function above.
 */
static int translate_sysarg(struct tracee_info *tracee, enum sysarg sysarg, int deref_final)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(tracee, old_path, sysarg);
	if (status < 0)
		return status;

	return translate_path2(tracee, AT_FDCWD, old_path, sysarg, deref_final);
}

/**
 * Detranslate the path pointed to by @addr in the @tracee's memory
 * space (@size bytes max). It returns the number of bytes used by the
 * new path pointed to by @addr, including the string terminator.
 *
 * Keep in sync' with sysnum_exit.c
 */
#define GETCWD   1
#define READLINK 2
static int detranslate_addr(struct tracee_info *tracee, word_t addr, int size, int mode)
{
	char path[PATH_MAX];
	int new_size;
	int status;

	assert(size >= 0);

	/* Extract the original path, be careful since some syscalls
	 * like readlink*(2) do not put a C string terminator. */
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	status = copy_from_tracee(tracee, path, addr, size);
	if (status < 0)
		return status;
	path[size] = '\0';

	/* Removes the leading "root" part. */
	status = detranslate_path(path, mode == GETCWD);
	if (status < 0)
		return status;
	new_size = status;

	/* The original path doesn't need detranslation, i.e it is a
	 * symetric binding. */
	if (new_size == 0) {
		new_size = size;
		goto skip_overwrite;
	}

	if (mode == GETCWD) {
		if (new_size > size)
			return -ERANGE;
	}
	else { /* READLINK */
		/* Don't include the terminating '\0'. */
		new_size--;
		if (new_size > size)
			new_size = size;
	}

	/* Overwrite the path. */
	status = copy_to_tracee(tracee, addr, path, new_size);
	if (status < 0)
		return status;

skip_overwrite:
	return new_size;
}

/**
 * Translate the input arguments of the syscall @tracee->sysnum in the
 * @tracee->pid process area. This function optionnaly sets
 * @tracee->output to the address of the output argument in the tracee's
 * memory space, it also sets @tracee->status to -errno if an error
 * occured from the tracee's perspective (EFAULT for instance),
 * otherwise to the amount of bytes "allocated" in the tracee's stack
 * for storing the newly translated paths. This function returns
 * -errno if an error occured from PRoot's perspective, otherwise 0.
 */
static int translate_syscall_enter(struct tracee_info *tracee)
{
	int flags;
	int dirfd;
	int olddirfd;
	int newdirfd;

	int status;
	int status1;
	int status2;

	char path[PATH_MAX];
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	tracee->sysnum = peek_ureg(tracee, SYSARG_NUM);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	if (config.verbose_level >= 3)
		VERBOSE(3, "pid %d: syscall(%ld, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) [0x%lx]",
			tracee->pid, tracee->sysnum,
			peek_ureg(tracee, SYSARG_1), peek_ureg(tracee, SYSARG_2),
			peek_ureg(tracee, SYSARG_3), peek_ureg(tracee, SYSARG_4),
			peek_ureg(tracee, SYSARG_5), peek_ureg(tracee, SYSARG_6),
			peek_ureg(tracee, STACK_POINTER));
	else
		VERBOSE(2, "pid %d: syscall(%d)", tracee->pid, (int)tracee->sysnum);

	/* Ensure one is not trying to cheat PRoot by calling an
	 * unsupported syscall on that architecture. */
	if ((int)tracee->sysnum < 0) {
		notice(WARNING, INTERNAL, "forbidden syscall %d", (int)tracee->sysnum);
		status = -ENOSYS;
		goto end;
	}

	/* Translate input arguments. */
	if (tracee->uregs == uregs) {
		#include SYSNUM_HEADER
		#include "syscall/enter.c"
	}
#ifdef SYSNUM_HEADER2
	else if (tracee->uregs == uregs2) {
		#include SYSNUM_HEADER2
		#include "syscall/enter.c"
	}
#endif
	#include "syscall/sysnum-undefined.h"
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", tracee->uregs);
		status = -ENOSYS;
	}

end:

	/* Remember the tracee status for the "exit" stage. */
	tracee->status = status;

	/* Avoid the actual syscall if an error occured during the
	 * translation. */
	if (status < 0) {
		status = poke_ureg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
		if (status < 0)
			return status;

		tracee->sysnum = SYSCALL_AVOIDER;
	}

	return 0;
}

/**
 * Check if the current syscall of @tracee actually is execve(2)
 * regarding the current ABI.
 */
static int is_execve(struct tracee_info *tracee)
{
	if (tracee->uregs == uregs) {
		#include SYSNUM_HEADER
		return tracee->sysnum == PR_execve;
	}
#ifdef SYSNUM_HEADER2
	else if (tracee->uregs == uregs2) {
		#include SYSNUM_HEADER2
		return tracee->sysnum == PR_execve;
	}
#endif
	#include "syscall/sysnum-undefined.h"
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", tracee->uregs);
		return 0;
	}
}

/**
 * Translate the output arguments of the syscall @tracee->sysnum in the
 * @tracee->pid process area. This function optionally detranslates the
 * path stored at @tracee->output in the tracee's memory space, it also
 * sets the result of this syscall to @tracee->status if an error
 * occured previously during the translation, that is, if
 * @tracee->status is less than 0, otherwise @tracee->status bytes of
 * the tracee's stack are "deallocated" to free the space used to store
 * the previously translated paths.
 */
static int translate_syscall_exit(struct tracee_info *tracee)
{
	word_t result;
	int status;

	/* Check if the process is still alive. */
	(void) peek_ureg(tracee, STACK_POINTER);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	/* Set the tracee's errno if an error occured previously during
	 * the translation. */
	if (tracee->status < 0) {
		status = poke_ureg(tracee, SYSARG_RESULT, (word_t) tracee->status);
		if (status < 0)
			goto end;

		result = (word_t) tracee->status;
	}
	else {
		result = peek_ureg(tracee, SYSARG_RESULT);
		if (errno != 0) {
			status = -errno;
			goto end;
		}
	}

	/* De-allocate the space used to store the previously
	 * translated paths. */
	if (tracee->status > 0
	    /* Restore the stack for execve() only if an error occured. */
	    && (!is_execve(tracee) || (int) result < 0)) {
		word_t tracee_ptr;
		tracee_ptr = resize_tracee_stack(tracee, -tracee->status);
		if (tracee_ptr == 0) {
			status = poke_ureg(tracee, SYSARG_RESULT, (word_t) -EFAULT);
			if (status < 0)
				goto end;
		}
	}

	VERBOSE(3, "pid %d:        -> 0x%lx [0x%lx]", tracee->pid, result,
		peek_ureg(tracee, STACK_POINTER));

	/* Translate output arguments. */
	if (tracee->uregs == uregs) {
		#include SYSNUM_HEADER
		#include "syscall/exit.c"
	}
#ifdef SYSNUM_HEADER2
	else if (tracee->uregs == uregs2) {
		#include SYSNUM_HEADER2
		#include "syscall/exit.c"
	}
#endif
	#include "syscall/sysnum-undefined.h"
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", tracee->uregs);
		status = -ENOSYS;
	}

	/* "status" was updated in sysnum_exit.c.  */
	status = poke_ureg(tracee, SYSARG_RESULT, (word_t) status);
end:
	if (status > 0)
		status = 0;

	/* Reset the current syscall number. */
	tracee->sysnum = -1;

	/* The struct tracee_info will be freed in
	 * main_loop() if status < 0. */
	return status;
}

int translate_syscall(pid_t pid)
{
	struct tracee_info *tracee;
	int status __attribute__ ((unused));

#ifdef BENCHMARK_PTRACE
	/* Check if the process is still alive. */
	(void) ptrace(PTRACE_PEEKUSER, pid, uregs[STACK_POINTER], NULL);
	return -errno;
#endif

	/* Get the information about this tracee. */
	tracee = get_tracee_info(pid);
	assert(tracee != NULL);

#ifdef ARCH_X86_64
	/* Check the current ABI. */
	status = ptrace(PTRACE_PEEKUSER, tracee->pid, USER_REGS_OFFSET(cs), NULL);
	if (status < 0)
		return status;
	if (status == 0x23)
		tracee->uregs = uregs2;
	else
		tracee->uregs = uregs;
#else
	tracee->uregs = uregs;
#endif /* ARCH_X86_64 */

	/* Check if we are either entering or exiting a syscall. */
	if (tracee->sysnum == (word_t) -1)
		return translate_syscall_enter(tracee);
	else
		return translate_syscall_exit(tracee);
}
