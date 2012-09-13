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
#include <stddef.h>      /* intptr_t, offsetof(3), getenv(2), */
#include <errno.h>       /* errno(3), */
#include <sys/user.h>    /* struct user*, */
#include <stdlib.h>      /* exit(3), strtoul(3), */
#include <string.h>      /* strlen(3), */
#include <sys/utsname.h> /* struct utsname, */
#include <stdarg.h>      /* va_*, */
#include <linux/version.h> /* KERNEL_VERSION, */
#include <talloc.h>      /* talloc_*, */

#include "syscall/syscall.h"
#include "arch.h"
#include "tracee/mem.h"
#include "tracee/tracee.h"
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
int get_sysarg_path(const struct tracee *tracee, char path[PATH_MAX], enum reg reg)
{
	int size;
	word_t src;

	src = peek_reg(tracee, CURRENT, reg);

	/* Check if the parameter is not NULL. Technically we should
	 * not return an -EFAULT for this special value since it is
	 * allowed for some syscall, utimensat(2) for instance. */
	if (src == 0) {
		path[0] = '\0';
		return 0;
	}

	/* Get the path from the tracee's memory space. */
	size = read_string(tracee, path, src, PATH_MAX);
	if (size < 0)
		return size;
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	path[size] = '\0';
	return size;
}

/**
 * Copy @size bytes of the data pointed to by @tracer_ptr into a
 * @tracee's memory block and make the @reg argument of the current
 * syscall points to this new block.  This function returns -errno if
 * an error occured, otherwise 0.
 */
int set_sysarg_data(struct tracee *tracee, void *tracer_ptr, word_t size, enum reg reg)
{
	word_t tracee_ptr;
	int status;

	/* Allocate space into the tracee's memory to host the new data. */
	tracee_ptr = alloc_mem(tracee, size);
	if (tracee_ptr == 0)
		return -EFAULT;

	/* Copy the new data into the previously allocated space. */
	status = write_data(tracee, tracee_ptr, tracer_ptr, size);
	if (status < 0)
		return status;

	/* Make this argument point to the new data. */
	poke_reg(tracee, reg, tracee_ptr);

	return 0;
}

/**
 * Copy @path to a @tracee's memory block and make the @reg argument
 * of the current syscall points to this new block.  This function
 * returns -errno if an error occured, otherwise 0.
 */
int set_sysarg_path(struct tracee *tracee, char path[PATH_MAX], enum reg reg)
{
	return set_sysarg_data(tracee, path, strlen(path) + 1, reg);
}

/**
 * Translate @path and put the result in the @tracee's memory address
 * space pointed to by the @reg argument of the current
 * syscall. See the documentation of translate_path() about the
 * meaning of @deref_final. This function returns -errno if an error
 * occured, otherwise 0.
 */
static int translate_path2(struct tracee *tracee, int dir_fd, char path[PATH_MAX], enum reg reg, int deref_final)
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
static int translate_sysarg(struct tracee *tracee, enum reg reg, int deref_final)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(tracee, old_path, reg);
	if (status < 0)
		return status;

	return translate_path2(tracee, AT_FDCWD, old_path, reg, deref_final);
}

static int parse_kernel_release(const char *release)
{
	unsigned long major = 0;
	unsigned long minor = 0;
	unsigned long revision = 0;
	char *cursor = (char *)release;

	major = strtoul(cursor, &cursor, 10);

	if (*cursor == '.') {
		cursor++;
		minor = strtoul(cursor, &cursor, 10);
	}

	if (*cursor == '.') {
		cursor++;
		revision = strtoul(cursor, &cursor, 10);
	}

	return KERNEL_VERSION(major, minor, revision);
}

#define MAX_ARG_SHIFT 2
struct syscall_modification {
	int expected_release;
	word_t new_sysarg_num;
	struct {
		enum reg sysarg;    /* first argument to be moved.  */
		size_t nb_args;     /* number of arguments to be moved.  */
		int offset;         /* offset to be applied.  */
	} shifts[MAX_ARG_SHIFT];
};

/**
 * Modify the current syscall of @tracee as described by @modif.
 *
 * This function return -errno if an error occured, otherwise 0.
 */
static int modify_syscall(struct tracee *tracee, struct syscall_modification *modif)
{
	static int emulated_release = -1;
	static int actual_release = -1;
	int status;
	int i;
	int j;

	assert(config.kernel_release != NULL);

	if (emulated_release < 0)
		emulated_release = parse_kernel_release(config.kernel_release);

	if (actual_release < 0) {
		struct utsname utsname;

		status = uname(&utsname);
		if (status < 0 || getenv("PROOT_FORCE_SYSCALL_COMPAT") != NULL)
			actual_release = 0;
		else
			actual_release = parse_kernel_release(utsname.release);
	}

	/* Emulate only syscalls that are available in the expected
	 * release but that are missing in the actual one.  */
	if (   modif->expected_release <= actual_release
	    || modif->expected_release > emulated_release)
		return 0;

	poke_reg(tracee, SYSARG_NUM, modif->new_sysarg_num);

	/* Shift syscall arguments.  */
	for (i = 0; i < MAX_ARG_SHIFT; i++) {
		enum reg sysarg    = modif->shifts[i].sysarg;
		size_t nb_args     = modif->shifts[i].nb_args;
		int offset         = modif->shifts[i].offset;

		for (j = 0; j < nb_args; j++) {
			word_t arg = peek_reg(tracee, CURRENT, sysarg + j);
			poke_reg(tracee, sysarg + j + offset, arg);
		}
	}
	status = 0;

	return status;
}

/**
 * Translate the input arguments of the current @tracee's syscall in the
 * @tracee->pid process area. This function sets @tracee->status to
 * -errno if an error occured from the tracee's point-of-view (EFAULT
 * for instance), otherwise 0. This function returns -errno if an
 * error occured from PRoot's point-of-view, otherwise 0.
 */
static int translate_syscall_enter(struct tracee *tracee)
{
	int flags;
	int dirfd;
	int olddirfd;
	int newdirfd;

	int status;

	char path[PATH_MAX];
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	if (config.verbose_level >= 3)
		VERBOSE(3, "pid %d: syscall(%ld, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) [0x%lx]",
			tracee->pid, peek_reg(tracee, CURRENT, SYSARG_NUM),
			peek_reg(tracee, CURRENT, SYSARG_1), peek_reg(tracee, CURRENT, SYSARG_2),
			peek_reg(tracee, CURRENT, SYSARG_3), peek_reg(tracee, CURRENT, SYSARG_4),
			peek_reg(tracee, CURRENT, SYSARG_5), peek_reg(tracee, CURRENT, SYSARG_6),
			peek_reg(tracee, CURRENT, STACK_POINTER));
	else
		VERBOSE(2, "pid %d: syscall(%d)", tracee->pid, (int)peek_reg(tracee, CURRENT, SYSARG_NUM));

	/* Translate input arguments. */
	switch (get_abi(tracee)) {
	case ABI_DEFAULT: {
		#include SYSNUM_HEADER

		#include "syscall/enter.c"

		if (config.kernel_release != NULL && status >= 0) {
			/* Errors are ignored.  */
			#include "syscall/compat.c"
		}
	}
		break;
#ifdef SYSNUM_HEADER2
	case ABI_2: {
		#include SYSNUM_HEADER2

		#include "syscall/enter.c"

		if (config.kernel_release != NULL && status >= 0) {
			/* Errors are ignored.  */
			#include "syscall/compat.c"
		}
	}
		break;
#endif
	default:
		assert(0);
	}
	#include "syscall/sysnum-undefined.h"

	/* Remember the tracee status for the "exit" stage and avoid
	 * the actual syscall if an error occured during the
	 * translation. */
	if (status < 0) {
		poke_reg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
		tracee->status = status;
	}
	else
		tracee->status = 1;

	return 0;
}

/**
 * Translate the output arguments of the current @tracee's syscall in
 * the @tracee->pid process area. This function sets the result of
 * this syscall to @tracee->status if an error occured previously
 * during the translation, that is, if @tracee->status is less than 0.
 */
static int translate_syscall_exit(struct tracee *tracee)
{
	bool restore_original_sp = true;
	int status;

	/* Set the tracee's errno if an error occured previously during
	 * the translation. */
	if (tracee->status < 0) {
		poke_reg(tracee, SYSARG_RESULT, (word_t) tracee->status);
		status = 0;
		goto end;
	}

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
	/* "restore_original_sp" was updated in syscall/exit.c.  */
	if (restore_original_sp)
		poke_reg(tracee, STACK_POINTER, peek_reg(tracee, ORIGINAL, STACK_POINTER));

	VERBOSE(3, "pid %d:        -> 0x%lx [0x%lx]", tracee->pid,
		peek_reg(tracee, CURRENT, SYSARG_RESULT),
		peek_reg(tracee, CURRENT, STACK_POINTER));

	if (status > 0)
		status = 0;

	/* Reset the tracee's status. */
	tracee->status = 0;

	/* The struct tracee will be freed in
	 * event_loop() if status < 0. */
	return status;
}

int translate_syscall(struct tracee *tracee)
{
	int result;
	int status;

	status = fetch_regs(tracee);
	if (status < 0)
		return status;

	tracee->tmp = talloc_new(tracee);

	/* Check if we are either entering or exiting a syscall. */
	result = (tracee->status == 0
		  ? translate_syscall_enter(tracee)
		  : translate_syscall_exit(tracee));

	TALLOC_FREE(tracee->tmp);

	status = push_regs(tracee);
	if (status < 0)
		return status;

	return result;
}
