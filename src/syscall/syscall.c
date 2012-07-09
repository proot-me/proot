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

#define _GNU_SOURCE      /* O_NOFOLLOW in fcntl.h, */
#include <fcntl.h>       /* AT_FDCWD, */
#include <sys/types.h>   /* pid_t, */
#include <assert.h>      /* assert(3), */
#include <limits.h>      /* PATH_MAX, */
#include <stddef.h>      /* intptr_t, offsetof(3), */
#include <errno.h>       /* errno(3), */
#include <sys/user.h>    /* struct user*, */
#include <stdlib.h>      /* exit(3), */
#include <string.h>      /* strlen(3), */
#include <sys/utsname.h> /* struct utsname, */

#include "syscall/syscall.h"
#include "arch.h"
#include "tracee/mem.h"
#include "tracee/info.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "path/path.h"
#include "execve/execve.h"
#include "notice.h"
#include "config.h"

#include "compat.h"

/**
 * Copy in @path a C string (PATH_MAX bytes max.) from the @tracee's
 * memory address space pointed to by the @reg argument of the
 * current syscall.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes put into the @path.
 */
int get_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum reg reg)
{
	int size;
	word_t src;

	src = peek_reg(tracee, reg);

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
 * Copy @size bytes of the data pointed to by @tracer_ptr to a
 * @tracee's memory block allocated below its stack and make the
 * @reg argument of the current syscall points to this new block.
 * This function returns -errno if an error occured, otherwise it
 * returns the size in bytes "allocated" into the stack.
 */
int set_sysarg_data(struct tracee_info *tracee, void *tracer_ptr, word_t size, enum reg reg)
{
	word_t tracee_ptr;
	int status;

	/* Allocate space into the tracee's stack to host the new data. */
	tracee_ptr = resize_tracee_stack(tracee, size);
	if (tracee_ptr == 0)
		return -EFAULT;

	/* Copy the new data into the previously allocated space. */
	status = copy_to_tracee(tracee, tracee_ptr, tracer_ptr, size);
	if (status < 0) {
		(void) resize_tracee_stack(tracee, -size);
		return status;
	}

	/* Make this argument point to the new data. */
	poke_reg(tracee, reg, tracee_ptr);

	return size;
}

/**
 * Copy @path to a @tracee's memory block allocated below its stack
 * and make the @reg argument of the current syscall points to this
 * new block.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes "allocated" into the stack.
 */
int set_sysarg_path(struct tracee_info *tracee, char path[PATH_MAX], enum reg reg)
{
	return set_sysarg_data(tracee, path, strlen(path) + 1, reg);
}

/**
 * Translate @path and put the result in the @tracee's memory address
 * space pointed to by the @reg argument of the current
 * syscall. See the documentation of translate_path() about the
 * meaning of @deref_final. This function returns -errno if an error
 * occured, otherwise it returns the size in bytes "allocated" into
 * the stack.
 */
static int translate_path2(struct tracee_info *tracee, int dir_fd, char path[PATH_MAX], enum reg reg, int deref_final)
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

	return set_sysarg_path(tracee, new_path, reg);
}

/**
 * A helper, see the comment of the function above.
 */
static int translate_sysarg(struct tracee_info *tracee, enum reg reg, int deref_final)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(tracee, old_path, reg);
	if (status < 0)
		return status;

	return translate_path2(tracee, AT_FDCWD, old_path, reg, deref_final);
}

/**
 * Translate the input arguments of the syscall @tracee->sysnum in the
 * @tracee->pid process area. This function sets @tracee->status to
 * -errno if an error occured from the tracee's perspective (EFAULT
 * for instance), otherwise to the amount of bytes "allocated" in the
 * tracee's stack for storing the newly translated paths. This
 * function returns -errno if an error occured from PRoot's
 * perspective, otherwise 0.
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

	tracee->sysnum = peek_reg(tracee, SYSARG_NUM);

	if (config.verbose_level >= 3)
		VERBOSE(3, "pid %d: syscall(%ld, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) [0x%lx]",
			tracee->pid, tracee->sysnum,
			peek_reg(tracee, SYSARG_1), peek_reg(tracee, SYSARG_2),
			peek_reg(tracee, SYSARG_3), peek_reg(tracee, SYSARG_4),
			peek_reg(tracee, SYSARG_5), peek_reg(tracee, SYSARG_6),
			peek_reg(tracee, STACK_POINTER));
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
	switch (get_abi(tracee)) {
	case ABI_DEFAULT: {
		#include SYSNUM_HEADER
		#include "syscall/enter.c"
	}
		break;
#ifdef SYSNUM_HEADER2
	case ABI_2: {
		#include SYSNUM_HEADER2
		#include "syscall/enter.c"
	}
		break;
#endif
	default:
		assert(0);
	}
	#include "syscall/sysnum-undefined.h"

end:

	/* Remember the tracee status for the "exit" stage. */
	tracee->status = status;

	/* Avoid the actual syscall if an error occured during the
	 * translation. */
	if (status < 0) {
		poke_reg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
		tracee->sysnum = SYSCALL_AVOIDER;
	}

	return 0;
}

/**
 * Check if the current syscall of @tracee actually is execve(2)
 * regarding the current ABI.
 */
static bool is_execve(struct tracee_info *tracee)
{
	switch (get_abi(tracee)) {
	case ABI_DEFAULT: {
		#include SYSNUM_HEADER
		return tracee->sysnum == PR_execve;
	}
#ifdef SYSNUM_HEADER2
	case ABI_2: {
		#include SYSNUM_HEADER2
		return tracee->sysnum == PR_execve;
	}
#endif
	default:
		assert(0);
	}
	#include "syscall/sysnum-undefined.h"
}

/**
 * Translate the output arguments of the syscall @tracee->sysnum in
 * the @tracee->pid process area. This function sets the result of
 * this syscall to @tracee->status if an error occured previously
 * during the translation, that is, if @tracee->status is less than 0,
 * otherwise @tracee->status bytes of the tracee's stack are
 * "deallocated" to free the space used to store the previously
 * translated paths.
 */
static int translate_syscall_exit(struct tracee_info *tracee)
{
	word_t result;
	int status;

	/* Set the tracee's errno if an error occured previously during
	 * the translation. */
	if (tracee->status < 0)
		poke_reg(tracee, SYSARG_RESULT, (word_t) tracee->status);
	result = peek_reg(tracee, SYSARG_RESULT);

	/* De-allocate the space used to store the previously
	 * translated paths. */
	if (tracee->status > 0
	    /* Restore the stack for execve() only if an error occured. */
	    && (!is_execve(tracee) || (int) result < 0)) {
		word_t tracee_ptr;
		tracee_ptr = resize_tracee_stack(tracee, -tracee->status);
		if (tracee_ptr == 0)
			poke_reg(tracee, SYSARG_RESULT, (word_t) -EFAULT);
	}

	VERBOSE(3, "pid %d:        -> 0x%lx [0x%lx]", tracee->pid, result,
		peek_reg(tracee, STACK_POINTER));

	/* Translate output arguments. */
	switch (get_abi(tracee)) {
	case ABI_DEFAULT: {
		#include SYSNUM_HEADER
		#include "syscall/exit.c"
	}
		break;
#ifdef SYSNUM_HEADER2
	case ABI_2: {
		#include SYSNUM_HEADER2
		#include "syscall/exit.c"
	}
		break;
#endif
	default:
		assert(0);
	}
	#include "syscall/sysnum-undefined.h"

	/* "status" was updated in syscall/exit.c.  */
	poke_reg(tracee, SYSARG_RESULT, (word_t) status);
	status = 0;
end:
	if (status > 0)
		status = 0;

	/* Reset the current syscall number. */
	tracee->sysnum = -1;

	/* The struct tracee_info will be freed in
	 * main_loop() if status < 0. */
	return status;
}

int translate_syscall(struct tracee_info *tracee)
{
	int result;
	int status;

	status = fetch_regs(tracee);
	if (status < 0)
		return status;

	/* Check if we are either entering or exiting a syscall. */
	result = (tracee->sysnum == (word_t) -1
		  ? translate_syscall_enter(tracee)
		  : translate_syscall_exit(tracee));

	status = push_regs(tracee);
	if (status < 0)
		return status;

	return result;
}
