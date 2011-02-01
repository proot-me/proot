/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
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
#include <stddef.h>      /* intptr_t, */
#include <errno.h>       /* errno(3), */
#include <sys/ptrace.h>  /* ptrace(2), PTRACE_*, */
#include <sys/user.h>    /* struct user*, */
#include <stdlib.h>      /* exit(3), */
#include <string.h>      /* strlen(3), */
#ifndef OLD_BROKEN_DISTRO
#include <sys/inotify.h> /* IN_DONT_FOLLOW, */
#endif

#include "syscall.h"
#include "arch.h"
#include "child_mem.h"
#include "child_info.h"
#include "path.h"
#include "execve.h"
#include "notice.h"
#include "ureg.h"

#include "compat.h"

static int allow_unknown = 0;
static int allow_ptrace = 0;

/**
 * Initialize internal data of the syscall module.
 */
void init_module_syscall(int sanity_check, int opt_allow_unknown, int opt_allow_ptrace)
{
	if (sanity_check != 0)
		notice(WARNING, USER, "option -s not yet supported");

	allow_unknown = opt_allow_unknown;
	allow_ptrace = opt_allow_ptrace;
}

/**
 * Copy in @path a C string (PATH_MAX bytes max.) from the @child's
 * memory address space pointed to by the @sysarg argument of the
 * current syscall.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes put into the @path.
 */
int get_sysarg_path(struct child_info *child, char path[PATH_MAX], enum sysarg sysarg)
{
	int size;
	word_t src;

	src = peek_ureg(child, sysarg);
	if (errno != 0)
		return -errno;

	/* Check if the parameter is not NULL. Technically we should
	 * not return an -EFAULT for this special value since it is
	 * allowed for some syscall, utimensat(2) for instance. */
	if (src == 0) {
		path[0] = '\0';
		return 0;
	}

	/* Get the path from the child's memory space. */
	size = get_child_string(child, path, src, PATH_MAX);
	if (size < 0)
		return size;
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	path[size] = '\0';
	return size;
}

/**
 * Copy @path in the @child's memory address space pointed to by
 * the @sysarg argument of the current syscall.  This function returns
 * -errno if an error occured, otherwise it returns the size in bytes
 * "allocated" into the stack.
 */
int set_sysarg_path(struct child_info *child, char path[PATH_MAX], enum sysarg sysarg)
{
	word_t child_ptr;
	int status;
	int size;

	/* Allocate space into the child's stack to host the new path. */
	size = strlen(path) + 1;
	child_ptr = resize_child_stack(child, size);
	if (child_ptr == 0)
		return -EFAULT;

	/* Copy the new path into the previously allocated space. */
	status = copy_to_child(child, child_ptr, path, size);
	if (status < 0) {
		(void) resize_child_stack(child, -size);
		return status;
	}

	/* Make this argument point to the new path. */
	status = poke_ureg(child, sysarg, child_ptr);
	if (status < 0) {
		(void) resize_child_stack(child, -size);
		return status;
	}

	return size;
}

/**
 * Translate @path and put the result in the @child's memory address
 * space pointed to by the @sysarg argument of the current
 * syscall. See the documentation of translate_path() about the
 * meaning of @deref_final. This function returns -errno if an error
 * occured, otherwise it returns the size in bytes "allocated" into
 * the stack.
 */
static int translate_path2(struct child_info *child, int dir_fd, char path[PATH_MAX], enum sysarg sysarg, int deref_final)
{
	char new_path[PATH_MAX];
	int status;

	/* Special case where the argument was NULL. */
	if (path[0] == '\0')
		return 0;

	/* Translate the original path. */
	status = translate_path(child, new_path, dir_fd, path, deref_final);
	if (status < 0)
		return status;

	return set_sysarg_path(child, new_path, sysarg);
}

/**
 * A helper, see the comment of the function above.
 */
static int translate_sysarg(struct child_info *child, enum sysarg sysarg, int deref_final)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(child, old_path, sysarg);
	if (status < 0)
		return status;

	return translate_path2(child, AT_FDCWD, old_path, sysarg, deref_final);
}

/**
 * Detranslate the path pointed to by @addr in the @child's memory
 * space (@size bytes max). It returns the number of bytes used by the
 * new path pointed to by @addr, including the string terminator.
 */
#define GETCWD   1
#define READLINK 2
static int detranslate_addr(struct child_info *child, word_t addr, int size, int mode)
{
	char path[PATH_MAX];
	int status;

	assert(size >= 0);

	/* Extract the original path, be careful since some syscalls
	 * like readlink*(2) do not put a C string terminator. */
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	status = copy_from_child(child, path, addr, size);
	if (status < 0)
		return status;
	path[size] = '\0';

	/* Removes the leading "root" part. */
	status = detranslate_path(path, mode == GETCWD);
	if (status < 0)
		return status;

	size = strlen(path);

	/* Only getcwd(2) puts the terminating \0. */
	if (mode == GETCWD)
		size++;

	/* The original path doesn't need detranslation. */
	if (size == 0)
		goto skip_overwrite;

	/* Overwrite the path. */
	status = copy_to_child(child, addr, path, size);
	if (status < 0)
		return status;

skip_overwrite:
	return size;
}

/**
 * Translate the input arguments of the syscall @child->sysnum in the
 * @child->pid process area. This function optionnaly sets
 * @child->output to the address of the output argument in the child's
 * memory space, it also sets @child->status to -errno if an error
 * occured from the child's perspective (EFAULT for instance),
 * otherwise to the amount of bytes "allocated" in the child's stack
 * for storing the newly translated paths. This function returns
 * -errno if an error occured from PRoot's perspective, otherwise 0.
 */
static int translate_syscall_enter(struct child_info *child)
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

	child->sysnum = peek_ureg(child, SYSARG_NUM);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	if (verbose_level >= 3)
		VERBOSE(3, "pid %d: syscall(%ld, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) [0x%lx]",
			child->pid, child->sysnum,
			peek_ureg(child, SYSARG_1), peek_ureg(child, SYSARG_2),
			peek_ureg(child, SYSARG_3), peek_ureg(child, SYSARG_4),
			peek_ureg(child, SYSARG_5), peek_ureg(child, SYSARG_6),
			peek_ureg(child, STACK_POINTER));
	else
		VERBOSE(2, "pid %d: syscall(%d)", child->pid, (int)child->sysnum);

	/* Ensure one is not trying to cheat PRoot by calling an
	 * unsupported syscall on that architecture. */
	if ((int)child->sysnum < 0) {
		notice(WARNING, INTERNAL, "forbidden syscall %d", (int)child->sysnum);
		status = -ENOSYS;
		goto end;
	}

	/* Translate input arguments. */
	if (child->uregs == uregs) {
		#include SYSNUM_HEADER
		#include "sysnum_enter.c"
	}
#ifdef SYSNUM_HEADER2
	else if (child->uregs == uregs2) {
		#include SYSNUM_HEADER2
		#include "sysnum_enter.c"
	}
#endif
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", child->uregs);
		status = -ENOSYS;
	}

end:

	/* Remember the child status for the "exit" stage. */
	child->status = status;

	/* Avoid the actual syscall if an error occured during the
	 * translation. */
	if (status < 0) {
		status = poke_ureg(child, SYSARG_NUM, SYSCALL_AVOIDER);
		if (status < 0)
			return status;

		child->sysnum = SYSCALL_AVOIDER;
	}

	return 0;
}

/**
 * Check if the current syscall of @child actually is execve(2)
 * regarding the current ABI.
 */
static int is_execve(struct child_info *child)
{
	if (child->uregs == uregs) {
		#include SYSNUM_HEADER
		return child->sysnum == __NR_execve;
	}
#ifdef SYSNUM_HEADER2
	else if (child->uregs == uregs2) {
		#include SYSNUM_HEADER2
		return child->sysnum == __NR_execve;
	}
#endif
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", child->uregs);
		return 0;
	}
}

/**
 * Translate the output arguments of the syscall @child->sysnum in the
 * @child->pid process area. This function optionally detranslates the
 * path stored at @child->output in the child's memory space, it also
 * sets the result of this syscall to @child->status if an error
 * occured previously during the translation, that is, if
 * @child->status is less than 0, otherwise @child->status bytes of
 * the child's stack are "deallocated" to free the space used to store
 * the previously translated paths.
 */
static int translate_syscall_exit(struct child_info *child)
{
	word_t result;
	int status;

	/* Check if the process is still alive. */
	(void) peek_ureg(child, STACK_POINTER);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	/* Set the child's errno if an error occured previously during
	 * the translation. */
	if (child->status < 0) {
		status = poke_ureg(child, SYSARG_RESULT, (word_t) child->status);
		if (status < 0)
			goto end;

		result = (word_t) child->status;
	}
	else {
		result = peek_ureg(child, SYSARG_RESULT);
		if (errno != 0) {
			status = -errno;
			goto end;
		}
	}

	/* De-allocate the space used to store the previously
	 * translated paths. */
	if (child->status > 0
	    /* Restore the stack for execve() only if an error occured. */
	    && (!is_execve(child) || (int) result < 0)) {
		word_t child_ptr;
		child_ptr = resize_child_stack(child, -child->status);
		if (child_ptr == 0) {
			status = poke_ureg(child, SYSARG_RESULT, (word_t) -EFAULT);
			if (status < 0)
				goto end;
		}
	}

	VERBOSE(3, "pid %d:        -> 0x%lx [0x%lx]", child->pid, result,
		peek_ureg(child, STACK_POINTER));

	/* Translate output arguments. */
	if (child->uregs == uregs) {
		#include SYSNUM_HEADER
		#include "sysnum_exit.c"
	}
#ifdef SYSNUM_HEADER2
	else if (child->uregs == uregs2) {
		#include SYSNUM_HEADER2
		#include "sysnum_exit.c"
	}
#endif
	else {
		notice(WARNING, INTERNAL, "unknown ABI (%p)", child->uregs);
		status = -ENOSYS;
	}

	if (status < 0) {
		status = poke_ureg(child, SYSARG_RESULT, (word_t) status);
		if (status < 0)
			goto end;
	}

end:
	if (status > 0)
		status = 0;

	/* Reset the current syscall number. */
	child->sysnum = -1;

	/* The struct child_info will be freed in
	 * main_loop() if status < 0. */
	return status;
}

int translate_syscall(pid_t pid)
{
	struct child_info *child;
	int status __attribute__ ((unused));

	/* Get the information about this child. */
	child = get_child_info(pid);
	assert(child != NULL);

#ifdef ARCH_X86_64
	/* Check the current ABI. */
	status = ptrace(PTRACE_PEEKUSER, child->pid, USER_REGS_OFFSET(cs), NULL);
	if (status < 0)
		return status;
	if (status == 0x23)
		child->uregs = uregs2;
	else
		child->uregs = uregs;
#else
	child->uregs = uregs;
#endif /* ARCH_X86_64 */

	/* Check if we are either entering or exiting a syscall. */
	if (child->sysnum == (word_t) -1)
		return translate_syscall_enter(child);
	else
		return translate_syscall_exit(child);
}
