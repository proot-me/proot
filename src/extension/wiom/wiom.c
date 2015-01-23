/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of WioM.
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

#include <stdbool.h>		/* bool, */
#include <fcntl.h>		/* O_*, */
#include <unistd.h>		/* access(2), readlink(2), */
#include <linux/limits.h>	/* PATH_MAX, */
#include <talloc.h>		/* talloc(3), */
#include <errno.h>		/* E*, */
#include <sys/mman.h>		/* MAP_*, PROT_*, */

#include "extension/wiom/wiom.h"
#include "extension/wiom/event.h"
#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "syscall/sysnum.h"
#include "syscall/syscall.h"
#include "path/path.h"
#include "cli/note.h"
#include "arch.h"

/*

Not yet supported
=================

- creates: bind(AF_UNIX)
- gets metadata: getxattr, lgetxattr, fgetxattr, listxattr, llistxattr, flistxattr
- sets metadata: setxattr, lsetxattr, fsetxattr, removexattr, lremovexattr, fremovexattr
- sets metadata: flock
- gets content: readdir
- descriptor status: (O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, O_NONBLOCK)
- gets descriptor position: llseek
- sets descriptor position: llseek

*/

/**
 * Report "TRAVERSES" event for given @path.
 */
static void handle_host_path(Extension *extension, const char *path, bool is_final)
{
	Config *config = extension->config;
	Tracee *tracee = TRACEE(extension);

	if (tracee->exe == NULL)
		return;

	if (access(path, F_OK) != 0) {
#if 0
		if (TODO->config.record.failure)
			TODO;
#endif
		return;
	}

#if 0
	if (!TODO->config.record.success)
		return;
#endif

	record_event(config->shared, tracee->pid, TRAVERSES, path);
}

/**
 * Read symlink "/proc/{@tracee->pid}/fd/{peek_reg(@sysarg)}", and
 * copy its value in @path.  This function returns -errno if an error
 * occurred, otherwise 0.
 */
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

/**
 * Remember information that will be mutated by the current syscall
 * but that are required by handle_sysenter_end().
 */
static void handle_sysenter_end(Extension *extension)
{
	Config *config = extension->config;

	char path2[PATH_MAX];
	char path[PATH_MAX];
	Reg sysarg2 = 0;
	Reg sysarg = 0;

	Tracee *tracee;
	word_t sysnum;
	int status;

	tracee = TRACEE(extension);

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
		chop_finality(path);

		flags = peek_reg(tracee, ORIGINAL, sysarg + 1);

		config->syscall_creates_path = (access(path, F_OK) != 0 && (flags & O_CREAT) != 0);
		break;
	}

	case PR_renameat:	/* SYSARG_2: string -> SYSARG_4: string */
		sysarg++;
		sysarg2 += 2;
	case PR_rename:		/* SYSARG_1: string -> SYSARG_2: string */
		sysarg++;
		sysarg2 += 2;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			return;
		chop_finality(path);

		status = get_sysarg_path(tracee, path2, sysarg2);
		if (status < 0)
			return;
		chop_finality(path2);

		config->syscall_creates_path = (access(path2, F_OK) != 0);
		break;

	default:
		break;
	}

	return;
}

/**
 * Report most events.
 */
static void handle_sysexit_start(const Extension *extension)
{
	Config *config = extension->config;

	char path2[PATH_MAX];
	char path[PATH_MAX];
	Reg sysarg2 = 0;
	Reg sysarg = 0;

	Tracee *tracee;
	word_t sysnum;
	int status;

	tracee = TRACEE(extension);

	status = (int) peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if (status < 0 && status > -4096) {
#if 0
		if (TODO->config.record.failure)
			TODO;
#endif
		return;
	}

#if 0
	if (!TODO->config.record.success)
		return;
#endif

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
		chop_finality(path);

		/* This implies set_content for parent of path.  */
		record_event(config->shared, tracee->pid, CREATES, path);
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
		chop_finality(path);

		/* This implies set_content for parent of path.  */
		record_event(config->shared, tracee->pid, DELETES, path);
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
		chop_finality(path);

		status = get_sysarg_path(tracee, path2, sysarg2);
		if (status < 0)
			goto error;
		chop_finality(path2);

		/* This implies set_content for parent of path & path2.  */
		record_event(config->shared, tracee->pid,
			config->syscall_creates_path ? MOVE_CREATES : MOVE_OVERRIDES, path, path2);
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
		chop_finality(path);

		record_event(config->shared, tracee->pid, GETS_METADATA_OF, path);
		break;

	case PR_fstat:		/* SYSARG_1: descriptor */
	case PR_fstat64:	/* SYSARG_1: descriptor */
	case PR_oldfstat:	/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;
		chop_finality(path);

		record_event(config->shared, tracee->pid, GETS_METADATA_OF, path);
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
		chop_finality(path);

		record_event(config->shared, tracee->pid, SETS_METADATA_OF, path);
		break;

	case PR_fchown:		/* SYSARG_1: descriptor */
	case PR_fchown32:	/* SYSARG_1: descriptor */
	case PR_fchmod:		/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;
		chop_finality(path);

		record_event(config->shared, tracee->pid, SETS_METADATA_OF, path);
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
		chop_finality(path);

		record_event(config->shared, tracee->pid, GETS_CONTENT_OF, path);
		break;

	case PR_read:		/* SYSARG_1: descriptor */
	case PR_readv:		/* SYSARG_1: descriptor */
	case PR_preadv:		/* SYSARG_1: descriptor */
	case PR_getdents:	/* SYSARG_1: descriptor */
	case PR_getdents64:	/* SYSARG_1: descriptor */
		status = get_proc_fd_path(tracee, path, SYSARG_1);
		if (status < 0)
			goto error;
		chop_finality(path);

		record_event(config->shared, tracee->pid, GETS_CONTENT_OF, path);
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
		chop_finality(path);

		record_event(config->shared, tracee->pid, SETS_CONTENT_OF, path);
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
		chop_finality(path);

		record_event(config->shared, tracee->pid, SETS_CONTENT_OF, path);
		break;

	/**********************************************************************
	 * Misc.
	 **********************************************************************/

	case PR_execve:
		/* Note: this implies get_metadata & get_content.  */
		record_event(config->shared, tracee->pid, EXECUTES, tracee->exe);
		break;

	case PR_openat:
		sysarg++;
	case PR_open: {
		sysarg++;

		word_t flags;

		status = get_sysarg_path(tracee, path, sysarg);
		if (status < 0)
			goto error;
		chop_finality(path);

		flags = peek_reg(tracee, MODIFIED, sysarg + 1);

		if (config->syscall_creates_path)  {
			/* This implies set_content for parent of path.  */
			record_event(config->shared, tracee->pid, CREATES, path);
		}
		else {
			record_event(config->shared, tracee->pid, GETS_METADATA_OF, path);

			if ((flags & O_TRUNC) != 0)
				record_event(config->shared, tracee->pid, SETS_CONTENT_OF, path);
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
		chop_finality(path);

		if ((prot & PROT_EXEC) != 0 || (prot & PROT_READ) != 0)
			record_event(config->shared, tracee->pid, GETS_CONTENT_OF, path);

		if ((prot & PROT_WRITE) != 0 && (flags & MAP_PRIVATE) == 0)
			record_event(config->shared, tracee->pid, SETS_CONTENT_OF, path);

		break;
	}

	default:
		break;
	}

	return;

error:
	note(tracee, ERROR, INTERNAL, "wiom: can't check syscall %s made by %d\n",
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
int wiom_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	case INITIALIZATION: {
		Config *config;

		extension->filtered_sysnums = filtered_sysnums;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		config = extension->config;

		config->shared = talloc_zero(config, SharedConfig);
		if (config->shared == NULL)
			return -1;

		config->shared->options = (Options *) data1;
		talloc_steal(config->shared, config->shared->options);

		talloc_set_destructor(config->shared, report_events);
		return 0;
	}

	case SYSCALL_ENTER_END:
		handle_sysenter_end(extension);
		return 0;

	case SYSCALL_EXIT_START:
		handle_sysexit_start(extension);
		return 0;

	case HOST_PATH:
		handle_host_path(extension, (const char *) data1, (bool) data2);
		return 0;

	case INHERIT_PARENT: {
		const Tracee *parent = TRACEE(extension);
		const Tracee *child  = (Tracee *) data1;
		word_t flags = (word_t) data2;
		Config *config = extension->config;

		record_event(config->shared, parent->pid, CLONED, child->pid, flags);
		return 1;
	}

	case INHERIT_CHILD: {
		Config *parent_config = ((Extension *) data1)->config;
		Config *child_config;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		child_config = extension->config;

		child_config->shared = talloc_reference(child_config, parent_config->shared);
		return 0;
	}

	default:
		return 0;
	}
}
