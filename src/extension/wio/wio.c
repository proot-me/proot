/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2014 STMicroelectronics
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

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <talloc.h>
#include <sys/mman.h>
#include <sched.h>
#include <linux/limits.h>

#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "syscall/sysnum.h"
#include "syscall/syscall.h"
#include "cli/note.h"
#include "arch.h"
#include "attribute.h"


/*

Operations
==========

- create: creat, open(O_CREAT), openat(O_CREAT), mkdir, mkdirat,
          mknod, mknodat, symlink, symlinkat, link, linkat, rename,
          renameat

- access:

  - metadata:

    - get: stat, lstat, fstat, fstatat, access, faccessat, execve

    - set: chmod, fchmod, fchmodat, chown, lchown, fchown, fchownat*,
           utime, utimes, futimesat, utimensat

  - content:

    - get: getdents, read, pread, readv, preadv, execve, readlink,
           readlinkat, mmap(PROT_EXEC | PROT_READ)

    - set: write, pwrite, writev, pwritev, truncate, ftruncate,
           open(O_TRUNC), mmap(PROT_WRITE)

- delete: unlink, unlinkat, rmdir, rename, renameat


Not yet supported
=================

- create/bind(AF_UNIX)
- access/metadata/get/{getxattr,lgetxattr,fgetxattr,listxattr,llistxattr,flistxattr}
- access/metadata/set/{setxattr,lsetxattr,fsetxattr,removexattr,lremovexattr,fremovexattr}
- access/metadata/set/flock
- access/content/get/readdir
- access/descriptor/status (O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, O_NONBLOCK)
- access/descriptor/position/{get,set}/llseek


Profile switches
================

- print path traversal
- print path type
- print path metadata usage
- print process (clone, pid) information
- coalese information

*/

typedef enum {
	TRAVERSES,
	CREATES,
	DELETES,
	GETS_METADATA_OF,
	SETS_METADATA_OF,
	GETS_CONTENT_OF,
	SETS_CONTENT_OF,
	EXECUTES,
	MOVES,
	IS_CLONED,
} Action;

#define PRINT(string) do {						\
		path = va_arg(ap, const char *);			\
		fprintf(stderr, "%d %s %s\n", pid, string, path);	\
	} while (0);							\
	break;

static void report(pid_t pid, Action action, ...)
{
	const char *path2;
	const char *path;
	va_list ap;

	va_start(ap, action);

	switch (action) {
	case TRAVERSES: 	PRINT("traverses");
	case CREATES:		PRINT("creates");
	case DELETES:		PRINT("deletes");
	case GETS_METADATA_OF:	PRINT("gets metadata of");
	case SETS_METADATA_OF:	PRINT("sets metadata of");
	case GETS_CONTENT_OF:	PRINT("gets content of");
	case SETS_CONTENT_OF:	PRINT("sets content of");
	case EXECUTES:		PRINT("executes");

	case MOVES:
		path = va_arg(ap, const char *);
		path2 = va_arg(ap, const char *);
		fprintf(stderr, "%d moves %s to %s\n", pid, path, path2);
		break;

	case IS_CLONED: {
		pid_t child_pid = va_arg(ap, pid_t);
		word_t flags    = va_arg(ap, word_t);
		fprintf(stderr, "%d is cloned (%s) into %d\n", pid,
			(flags & CLONE_THREAD) != 0 ? "thread": "process", child_pid);
		break;
	}

	default:
		assert(0);
		break;
	}

	va_end(ap);

	return;
}

static void handle_host_path(Extension *extension, const char *path, bool is_final)
{
	Tracee *tracee;

	tracee = TRACEE(extension);

	if (tracee->exe == NULL)
		return;

	if (access(path, F_OK) != 0)
		return;

	report(tracee->pid, TRAVERSES, path);
}

static int get_proc_fd_path(const Tracee *tracee, char path[PATH_MAX], Reg sysarg)
{
	char *tmp;
	int status;

	tmp = talloc_asprintf(tracee->ctx, "/proc/%d/fd/%d",
			tracee->pid, (int) peek_reg(tracee, MODIFIED, sysarg));
	if (tmp == NULL)
		return -ENOMEM;

	status = readlink(tmp, path, PATH_MAX);
	if (status < 0 || status >= PATH_MAX)
		return -(errno ?: ENAMETOOLONG);
	path[status] = '\0';

	return 0;
}

static void handle_sysenter_end(Extension *extension)
{
	Reg sysarg = 0;

	char path[PATH_MAX];
	Tracee *tracee;
	word_t sysnum;
	int status;

	tracee = TRACEE(extension);
	extension->config = NULL;

	sysnum = get_sysnum(tracee, ORIGINAL);
	switch (sysnum) {
	case PR_openat:
		sysarg++;
	case PR_open: {
		sysarg++;

		word_t flags;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			return;

		flags = peek_reg(tracee, ORIGINAL, sysarg + 1);

		if (access(path, F_OK) != 0 && (flags & O_CREAT) != 0)
			extension->config = (void *) -1;

		break;
	}

	default:
		break;
	}

	return;
}

static void handle_sysexit_start(const Extension *extension)
{
	char path2[PATH_MAX];
	char path[PATH_MAX];
	Reg sysarg2 = 0;
	Reg sysarg = 0;

	Tracee *tracee;
	word_t sysnum;
	int status;

	tracee = TRACEE(extension);

	status = (int) peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if (status < 0 && status > -4096)
		return;

	sysnum = get_sysnum(tracee, MODIFIED);
	switch (sysnum) {
	/**********************************************************************
	 * Create
	 **********************************************************************/

	case PR_linkat:		/* SYSARG_4: string */
		sysarg++;
	case PR_symlinkat:	/* SYSARG_3: string */
		sysarg++;
	case PR_mkdirat:	/* SYSARG_2: string */
	case PR_mknodat:	/* SYSARG_2: string */
	case PR_symlink:	/* SYSARG_2: string */
	case PR_link:		/* SYSARG_2: string */
		sysarg++;
	case PR_creat:		/* SYSARG_1: string */
	case PR_mkdir:		/* SYSARG_1: string */
	case PR_mknod:		/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		/* This implies set_content for parent of path.  */
		report(tracee->pid, CREATES, path);
		break;

	/**********************************************************************
	 * Delete
	 **********************************************************************/

	case PR_unlinkat:	/* SYSARG_2: string */
		sysarg++;
	case PR_unlink:		/* SYSARG_1: string */
	case PR_rmdir:		/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		/* This implies set_content for parent of path.  */
		report(tracee->pid, DELETES, path);
		break;

	/**********************************************************************
	 * Move
	 **********************************************************************/

	case PR_renameat:	/* SYSARG_2: string -> SYSARG_4: string */
		sysarg++;
		sysarg2 += 2;
	case PR_rename:		/* SYSARG_1: string -> SYSARG_2: string */
		sysarg++;
		sysarg2 += 2;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		status = get_sysarg_path(tracee, path2, sysarg2);
		if (status < 0)
			goto error;

		/* This implies set_content for parent of path & path2.  */
		report(tracee->pid, MOVES, path, path2);
		break;

	/**********************************************************************
	 * Get metadata
	 **********************************************************************/

	case PR_fstatat64:	/* SYSARG_2: string */
	case PR_newfstatat:	/* SYSARG_2: string */
	case PR_faccessat:	/* SYSARG_2: string */
		sysarg++;
	case PR_access:		/* SYSARG_1: string */
	case PR_stat:		/* SYSARG_1: string */
	case PR_stat64:		/* SYSARG_1: string */
	case PR_oldstat:	/* SYSARG_1: string */
	case PR_lstat:		/* SYSARG_1: string */
	case PR_lstat64:	/* SYSARG_1: string */
	case PR_oldlstat:	/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		report(tracee->pid, GETS_METADATA_OF, path);
		break;

	case PR_fstat:		/* SYSARG_1: descriptor */
	case PR_fstat64:	/* SYSARG_1: descriptor */
	case PR_oldfstat:	/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;

		report(tracee->pid, GETS_METADATA_OF, path);
		break;

	/**********************************************************************
	 * Set metadata
	 **********************************************************************/

	case PR_fchmodat:	/* SYSARG_2: string */
	case PR_fchownat:	/* SYSARG_2: string */
	case PR_futimesat:	/* SYSARG_2: string */
	case PR_utimensat:	/* SYSARG_2: string */
		sysarg++;
	case PR_chmod:		/* SYSARG_1: string */
	case PR_chown:		/* SYSARG_1: string */
	case PR_lchown:		/* SYSARG_1: string */
	case PR_chown32:	/* SYSARG_1: string */
	case PR_lchown32:	/* SYSARG_1: string */
	case PR_utime:		/* SYSARG_1: string */
	case PR_utimes:		/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		report(tracee->pid, SETS_METADATA_OF, path);
		break;

	case PR_fchown:		/* SYSARG_1: descriptor */
	case PR_fchown32:	/* SYSARG_1: descriptor */
	case PR_fchmod:		/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;

		report(tracee->pid, SETS_METADATA_OF, path);
		break;

	/**********************************************************************
	 * Get content
	 **********************************************************************/

	case PR_readlinkat:	/* SYSARG_2: string */
		sysarg++;
	case PR_readlink:	/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		report(tracee->pid, GETS_CONTENT_OF, path);
		break;

	case PR_read:		/* SYSARG_1: descriptor */
	case PR_readv:		/* SYSARG_1: descriptor */
	case PR_preadv:		/* SYSARG_1: descriptor */
	case PR_getdents:	/* SYSARG_1: descriptor */
	case PR_getdents64:	/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;

		report(tracee->pid, GETS_CONTENT_OF, path);
		break;

	/**********************************************************************
	 * Set content
	 **********************************************************************/

	case PR_truncate:	/* SYSARG_1: string */
	case PR_truncate64:	/* SYSARG_1: string */
		sysarg++;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		report(tracee->pid, SETS_CONTENT_OF, path);
		break;

	case PR_write:		/* SYSARG_1: descriptor */
	case PR_writev:		/* SYSARG_1: descriptor */
	case PR_pwritev:	/* SYSARG_1: descriptor */
	case PR_pwrite64:	/* SYSARG_1: descriptor */
	case PR_ftruncate:	/* SYSARG_1: descriptor */
	case PR_ftruncate64:	/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;

		report(tracee->pid, SETS_CONTENT_OF, path);
		break;

	/**********************************************************************
	 * Misc.
	 **********************************************************************/

	case PR_execve:
		/* Note: this implies get_metadata & get_content.  */
		report(tracee->pid, EXECUTES, tracee->exe);
		break;

	case PR_openat:
		sysarg++;
	case PR_open: {
		sysarg++;

		word_t flags;
		bool didnt_exist;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;

		flags = peek_reg(tracee, MODIFIED, sysarg + 1);
		didnt_exist = (extension->config == (void *) -1);

		if (didnt_exist)  {
			/* This implies set_content for parent of path.  */
			report(tracee->pid, CREATES, path);
		}
		else {
			report(tracee->pid, GETS_METADATA_OF, path);

			if ((flags & O_TRUNC) != 0)
				report(tracee->pid, SETS_CONTENT_OF, path);
		}

		break;
	}

	case PR_mmap:
	case PR_mmap2: {
		word_t flags;
		word_t prot;

		prot  = peek_reg(tracee, MODIFIED, SYSARG_3);
		flags = peek_reg(tracee, MODIFIED, SYSARG_4);

		if ((flags & MAP_ANONYMOUS) != 0)
			break;

		status = get_proc_fd_path(tracee, path, SYSARG_5);
		if (status < 0)
			goto error;

		if ((prot & PROT_EXEC) != 0 || (prot & PROT_READ) != 0)
			report(tracee->pid, GETS_CONTENT_OF, path);

		if ((prot & PROT_WRITE) != 0 && (flags & MAP_PRIVATE) == 0)
			report(tracee->pid, SETS_CONTENT_OF, path);

		break;
	}

	default:
		break;
	}

	return;

error:
	note(tracee, ERROR, INTERNAL, "extension: can't check syscall %s made by %d\n",
		stringify_sysnum(sysnum), tracee->pid);
	return;
}

static FilteredSysnum filtered_sysnums[] = {
	{ PR_access,		FILTER_SYSEXIT },
	{ PR_chmod,		FILTER_SYSEXIT },
	{ PR_chown,		FILTER_SYSEXIT },
	{ PR_chown32,		FILTER_SYSEXIT },
	{ PR_creat,		FILTER_SYSEXIT },
	{ PR_execve,		FILTER_SYSEXIT },
	{ PR_faccessat,		FILTER_SYSEXIT },
	{ PR_fchmod,		FILTER_SYSEXIT },
	{ PR_fchmodat,		FILTER_SYSEXIT },
	{ PR_fchown,		FILTER_SYSEXIT },
	{ PR_fchown32,		FILTER_SYSEXIT },
	{ PR_fchownat,		FILTER_SYSEXIT },
	{ PR_fstat,		FILTER_SYSEXIT },
	{ PR_fstat64,		FILTER_SYSEXIT },
	{ PR_fstatat64,		FILTER_SYSEXIT },
	{ PR_ftruncate,		FILTER_SYSEXIT },
	{ PR_ftruncate64,	FILTER_SYSEXIT },
	{ PR_futimesat,		FILTER_SYSEXIT },
	{ PR_getdents,		FILTER_SYSEXIT },
	{ PR_getdents64,	FILTER_SYSEXIT },
	{ PR_lchown,		FILTER_SYSEXIT },
	{ PR_lchown32,		FILTER_SYSEXIT },
	{ PR_link,		FILTER_SYSEXIT },
	{ PR_linkat,		FILTER_SYSEXIT },
	{ PR_lstat,		FILTER_SYSEXIT },
	{ PR_lstat64,		FILTER_SYSEXIT },
	{ PR_mkdir,		FILTER_SYSEXIT },
	{ PR_mkdirat,		FILTER_SYSEXIT },
	{ PR_mknod,		FILTER_SYSEXIT },
	{ PR_mknodat,		FILTER_SYSEXIT },
	{ PR_mmap,		FILTER_SYSEXIT },
	{ PR_mmap2,		FILTER_SYSEXIT },
	{ PR_newfstatat,	FILTER_SYSEXIT },
	{ PR_oldfstat,		FILTER_SYSEXIT },
	{ PR_oldlstat,		FILTER_SYSEXIT },
	{ PR_oldstat,		FILTER_SYSEXIT },
	{ PR_open,		FILTER_SYSEXIT },
	{ PR_openat,		FILTER_SYSEXIT },
	{ PR_preadv,		FILTER_SYSEXIT },
	{ PR_pwrite64,		FILTER_SYSEXIT },
	{ PR_pwritev,		FILTER_SYSEXIT },
	{ PR_read,		FILTER_SYSEXIT },
	{ PR_readlink,		FILTER_SYSEXIT },
	{ PR_readlinkat,	FILTER_SYSEXIT },
	{ PR_readv,		FILTER_SYSEXIT },
	{ PR_rename,		FILTER_SYSEXIT },
	{ PR_renameat,		FILTER_SYSEXIT },
	{ PR_rmdir,		FILTER_SYSEXIT },
	{ PR_stat,		FILTER_SYSEXIT },
	{ PR_stat64,		FILTER_SYSEXIT },
	{ PR_symlink,		FILTER_SYSEXIT },
	{ PR_symlinkat,		FILTER_SYSEXIT },
	{ PR_truncate,		FILTER_SYSEXIT },
	{ PR_truncate64,	FILTER_SYSEXIT },
	{ PR_unlink,		FILTER_SYSEXIT },
	{ PR_unlinkat,		FILTER_SYSEXIT },
	{ PR_utime,		FILTER_SYSEXIT },
	{ PR_utimensat,		FILTER_SYSEXIT },
	{ PR_utimes,		FILTER_SYSEXIT },
	{ PR_write,		FILTER_SYSEXIT },
	{ PR_writev,		FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int wio_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	case INITIALIZATION:
		extension->filtered_sysnums = filtered_sysnums;
		return 0;

	case SYSCALL_ENTER_END:
		handle_sysenter_end(extension);
		return 0;

	case SYSCALL_EXIT_START:
		handle_sysexit_start(extension);
		return 0;

	case HOST_PATH:
		handle_host_path(extension, (const char *) data1, (bool) data2);
		return 0;

	case INHERIT_PARENT:
		return 1;

	case INHERIT_CHILD: {
		const Tracee *parent = TRACEE((Extension *) data1);
		const Tracee *child  = TRACEE(extension);
		word_t flags = (word_t) data2;

		report(parent->pid, IS_CLONED, child->pid, flags);
		return 0;
	}

	default:
		return 0;
	}
}
