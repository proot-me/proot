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

#define _GNU_SOURCE      /* O_NOFOLLOW in fcntl.h, clone(2), */
#include <sched.h>       /* clone(2), */
#include <fcntl.h>       /* AT_FDCWD, */
#include <sys/types.h>   /* pid_t, */
#include <assert.h>      /* assert(3), */
#include <limits.h>      /* PATH_MAX, */
#include <stddef.h>      /* intptr_t, offsetof(3), getenv(2), */
#include <errno.h>       /* errno(3), */
#include <string.h>      /* strlen(3), */
#include <sys/utsname.h> /* struct utsname, */
#include <stdarg.h>      /* va_*, */
#include <talloc.h>      /* talloc_*, */
#include <sys/un.h>      /* struct sockaddr_un, */
#include <linux/net.h>   /* SYS_*, */

#include "syscall/syscall.h"
#include "syscall/socket.h"
#include "arch.h"
#include "tracee/mem.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "path/path.h"
#include "path/canon.h"
#include "execve/execve.h"
#include "extension/extension.h"
#include "notice.h"

#include "compat.h"

/**
 * Copy in @path a C string (PATH_MAX bytes max.) from the @tracee's
 * memory address space pointed to by the @reg argument of the
 * current syscall.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes put into the @path.
 */
int get_sysarg_path(const Tracee *tracee, char path[PATH_MAX], Reg reg)
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
int set_sysarg_data(Tracee *tracee, void *tracer_ptr, word_t size, Reg reg)
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
int set_sysarg_path(Tracee *tracee, char path[PATH_MAX], Reg reg)
{
	return set_sysarg_data(tracee, path, strlen(path) + 1, reg);
}

/**
 * Translate @path and put the result in the @tracee's memory address
 * space pointed to by the @reg argument of the current syscall. See
 * the documentation of translate_path() about the meaning of
 * @type. This function returns -errno if an error occured, otherwise
 * 0.
 */
static int translate_path2(Tracee *tracee, int dir_fd, char path[PATH_MAX], Reg reg, Type type)
{
	char new_path[PATH_MAX];
	int status;

	/* Special case where the argument was NULL. */
	if (path[0] == '\0')
		return 0;

	/* Translate the original path. */
	status = translate_path(tracee, new_path, dir_fd, path, type != SYMLINK);
	if (status < 0)
		return status;

	return set_sysarg_path(tracee, new_path, reg);
}

/**
 * A helper, see the comment of the function above.
 */
static int translate_sysarg(Tracee *tracee, Reg reg, Type type)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(tracee, old_path, reg);
	if (status < 0)
		return status;

	return translate_path2(tracee, AT_FDCWD, old_path, reg, type);
}

#define SYSARG_ADDR(n) (args_addr + ((n) - 1) * sizeof_word(tracee))

#define PEEK_MEM(addr) peek_mem(tracee, addr);			\
	if (errno != 0) {					\
		status = -errno;				\
		break;						\
	}

#define POKE_MEM(addr, value) poke_mem(tracee, addr, value);	\
	if (errno != 0) {					\
		status = -errno;				\
		break;						\
	}

/**
 * Translate the input arguments of the current @tracee's syscall in the
 * @tracee->pid process area. This function sets @tracee->status to
 * -errno if an error occured from the tracee's point-of-view (EFAULT
 * for instance), otherwise 0.
 */
static void translate_syscall_enter(Tracee *tracee)
{
	int flags;
	int dirfd;
	int olddirfd;
	int newdirfd;

	int status;
	int status2;

	char path[PATH_MAX];
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	word_t syscall_number;

	status = notify_extensions(tracee, SYSCALL_ENTER_START, 0, 0);
	if (status < 0)
		goto end;
	if (status > 0)
		return;

	/* Translate input arguments. */
	switch (get_abi(tracee)) {
	case ABI_DEFAULT: {
		#include SYSNUM_HEADER
		#include "syscall/enter.c"
		break;
	}
#ifdef SYSNUM_HEADER2
	case ABI_2: {
		#include SYSNUM_HEADER2
		#include "syscall/enter.c"
		break;
	}
#endif
#ifdef SYSNUM_HEADER3
	case ABI_3: {
		#include SYSNUM_HEADER3
		#include "syscall/enter.c"
		break;
	}
#endif
	default:
		assert(0);
	}
	#include "syscall/sysnum-undefined.h"

end:
	status2 = notify_extensions(tracee, SYSCALL_ENTER_END, status, 0);
	if (status2 < 0)
		status = status2;

	/* Remember the tracee status for the "exit" stage and avoid
	 * the actual syscall if an error occured during the
	 * translation. */
	if (status < 0) {
		poke_reg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
		tracee->status = status;
	}
	else
		tracee->status = 1;

}

/**
 * Translate the output arguments of the current @tracee's syscall in
 * the @tracee->pid process area. This function sets the result of
 * this syscall to @tracee->status if an error occured previously
 * during the translation, that is, if @tracee->status is less than 0.
 */
static void translate_syscall_exit(Tracee *tracee)
{
	word_t syscall_number;
	word_t syscall_result;
	int status;

	status = notify_extensions(tracee, SYSCALL_EXIT_START, 0, 0);
	if (status < 0) {
		poke_reg(tracee, SYSARG_RESULT, (word_t) status);
		goto end;
	}
	if (status > 0)
		return;

	/* Set the tracee's errno if an error occured previously during
	 * the translation. */
	if (tracee->status < 0) {
		poke_reg(tracee, SYSARG_RESULT, (word_t) tracee->status);
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
#ifdef SYSNUM_HEADER3
	case ABI_3: {
		#include SYSNUM_HEADER3
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

end:
	status = notify_extensions(tracee, SYSCALL_EXIT_END, 0, 0);
	if (status < 0)
		poke_reg(tracee, SYSARG_RESULT, (word_t) status);

	/* Reset the tracee's status. */
	tracee->status = 0;
}

#undef SYSARG_ADDR
#undef PEEK_MEM
#undef POKE_MEM

int translate_syscall(Tracee *tracee)
{
	int status;

	status = fetch_regs(tracee);
	if (status < 0)
		return status;

	/* Check if we are either entering or exiting a syscall. */
	(tracee->status == 0
		  ? translate_syscall_enter(tracee)
		  : translate_syscall_exit(tracee));

	status = push_regs(tracee);
	if (status < 0)
		return status;

	return 0;
}
