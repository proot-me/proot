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

#include <assert.h>  /* assert(3), */
#include <stdint.h>  /* intptr_t, */
#include <errno.h>   /* E*, */
#include <sys/stat.h>   /* chmod(2), stat(2) */
#include <unistd.h>  /* get*id(2),  */
#include <sys/ptrace.h>    /* linux.git:c0a3a20b  */
#include <linux/audit.h>   /* AUDIT_ARCH_*,  */

#include "extension/extension.h"
#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "syscall/seccomp.h"
#include "tracee/tracee.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "path/binding.h"
#include "notice.h"
#include "arch.h"

typedef struct {
	char *path;
	mode_t mode;
} ModifiedNode;

/**
 * Restore the @node->mode for the given @node->path.
 *
 * Note: this is a Talloc destructor.
 */
static int restore_mode(ModifiedNode *node)
{
	(void) chmod(node->path, node->mode);
	return 0;
}

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_chmod,		FILTER_SYSEXIT },
	{ PR_chown,		FILTER_SYSEXIT },
	{ PR_chown32,		FILTER_SYSEXIT },
	{ PR_chroot,		FILTER_SYSEXIT },
	{ PR_fchmod,		FILTER_SYSEXIT },
	{ PR_fchmodat,		FILTER_SYSEXIT },
	{ PR_fchown,		FILTER_SYSEXIT },
	{ PR_fchown32,		FILTER_SYSEXIT },
	{ PR_fchownat,		FILTER_SYSEXIT },
	{ PR_fchownat,		FILTER_SYSEXIT },
	{ PR_fstat,		FILTER_SYSEXIT },
	{ PR_fstat,		FILTER_SYSEXIT },
	{ PR_fstat64,		FILTER_SYSEXIT },
	{ PR_fstatat64,		FILTER_SYSEXIT },
	{ PR_getegid,		FILTER_SYSEXIT },
	{ PR_getegid32,		FILTER_SYSEXIT },
	{ PR_geteuid,		FILTER_SYSEXIT },
	{ PR_geteuid32,		FILTER_SYSEXIT },
	{ PR_getgid,		FILTER_SYSEXIT },
	{ PR_getgid32,		FILTER_SYSEXIT },
	{ PR_getresgid,		FILTER_SYSEXIT },
	{ PR_getresgid32,	FILTER_SYSEXIT },
	{ PR_getresuid,		FILTER_SYSEXIT },
	{ PR_getresuid32,	FILTER_SYSEXIT },
	{ PR_getuid,		FILTER_SYSEXIT },
	{ PR_getuid32,		FILTER_SYSEXIT },
	{ PR_lchown,		FILTER_SYSEXIT },
	{ PR_lchown32,		FILTER_SYSEXIT },
	{ PR_lstat,		FILTER_SYSEXIT },
	{ PR_lstat64,		FILTER_SYSEXIT },
	{ PR_newfstatat,	FILTER_SYSEXIT },
	{ PR_oldlstat,		FILTER_SYSEXIT },
	{ PR_oldstat,		FILTER_SYSEXIT },
	{ PR_setfsgid,		FILTER_SYSEXIT },
	{ PR_setfsgid32,	FILTER_SYSEXIT },
	{ PR_setfsuid,		FILTER_SYSEXIT },
	{ PR_setfsuid32,	FILTER_SYSEXIT },
	{ PR_setgid,		FILTER_SYSEXIT },
	{ PR_setgid32,		FILTER_SYSEXIT },
	{ PR_setresgid,		FILTER_SYSEXIT },
	{ PR_setresgid32,	FILTER_SYSEXIT },
	{ PR_setresuid,		FILTER_SYSEXIT },
	{ PR_setresuid32,	FILTER_SYSEXIT },
	{ PR_setuid,		FILTER_SYSEXIT },
	{ PR_setuid32,		FILTER_SYSEXIT },
	{ PR_stat,		FILTER_SYSEXIT },
	{ PR_stat64,		FILTER_SYSEXIT },
	{ PR_statfs,		FILTER_SYSEXIT },
	{ PR_statfs64,		FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

/**
 * Force permissions of @path to "rwx" during the path translation of
 * current @tracee's syscall, in order to simulate CAP_DAC_OVERRIDE.
 * The original permissions are restored through talloc destructors.
 * See canonicalize() for the meaning of @is_final.
 */
static void handle_host_path(const Tracee *tracee, const char *path, bool is_final)
{
	ModifiedNode *node;
	struct stat perms;
	mode_t new_mode;
	int status;

	/* Get the meta-data */
	status = stat(path, &perms);
	if (status < 0)
		return;

	/* Copy the current permissions */
	new_mode = perms.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

	/* Add read and write permissions to everything.  */
	new_mode |= (S_IRUSR | S_IWUSR);

	/* Always add 'x' bit to directories */
	if (S_ISDIR(perms.st_mode))
		new_mode |= S_IXUSR;

	/* Patch the permissions only if needed.  */
	if (new_mode == (perms.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)))
		return;

	node = talloc_zero(tracee->ctx, ModifiedNode);
	if (node == NULL)
		return;

	if (!is_final) {
		/* Restore the previous mode of any non final components.  */
		node->mode = perms.st_mode;
	}
	else {
		switch (get_sysnum(tracee, ORIGINAL)) {
		/* For chmod syscalls: restore the new mode of the final component.  */
		case PR_chmod:
			node->mode = peek_reg(tracee, ORIGINAL, SYSARG_2);
			break;

		case PR_fchmodat:
			node->mode = peek_reg(tracee, ORIGINAL, SYSARG_3);
			break;

		/* For stat syscalls: don't touch the mode of the final component.  */
		case PR_fstatat64:
		case PR_lstat:
		case PR_lstat64:
		case PR_newfstatat:
		case PR_oldlstat:
		case PR_oldstat:
		case PR_stat:
		case PR_stat64:
		case PR_statfs:
		case PR_statfs64:
			return;

		/* Otherwise: restore the previous mode of the final component.  */
		default:
			node->mode = perms.st_mode;
			break;
		}
	}

	node->path = talloc_strdup(node, path);
	if (node->path == NULL) {
		/* Keep only consistent nodes.  */
		TALLOC_FREE(node);
		return;
	}

	/* The mode restoration works because Talloc destructors are
	 * called in reverse order.  */
	talloc_set_destructor(node, restore_mode);

	(void) chmod(path, new_mode);

	return;
}

/**
 * Force current @tracee's syscall to behave as if executed by "root".
 * This function returns -errno if an error occured, otherwise 0.
 */
static int handle_sysexit_end(Tracee *tracee)
{
	word_t sysnum;

	sysnum = get_sysnum(tracee, ORIGINAL);
	switch (sysnum) {
	case PR_chroot: {
		char path[PATH_MAX];
		word_t result;
		word_t input;
		int status;

		/* Override only permission errors.  */
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		if ((int) result != -EPERM)
			return 0;

		input = peek_reg(tracee, MODIFIED, SYSARG_1);

		status = read_string(tracee, path, input, PATH_MAX);
		if (status < 0)
			return status;

		/* Only "new rootfs == current rootfs" is supported yet.  */
		status = compare_paths(get_root(tracee), path);
		if (status != PATHS_ARE_EQUAL)
			return 0;

		/* Force success.  */
		poke_reg(tracee, SYSARG_RESULT, 0);
		return 0;
	}

	case PR_setresuid:
	case PR_setresgid:
	case PR_setresuid32:
	case PR_setresgid32:
	case PR_chmod:
	case PR_chown:
	case PR_fchmod:
	case PR_fchown:
	case PR_lchown:
	case PR_chown32:
	case PR_fchown32:
	case PR_lchown32:
	case PR_fchmodat:
	case PR_fchownat: {
		word_t result;

		/* Override only permission errors.  */
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		if ((int) result != -EPERM)
			return 0;

		/* Force success.  */
		poke_reg(tracee, SYSARG_RESULT, 0);
		return 0;
	}

	case PR_getresuid:
	case PR_getresuid32:
	case PR_getresgid:
	case PR_getresgid32:
		poke_mem(tracee, peek_reg(tracee, ORIGINAL, SYSARG_1), 0);
		if (errno != 0)
			return -EFAULT;

		poke_mem(tracee, peek_reg(tracee, ORIGINAL, SYSARG_2), 0);
		if (errno != 0)
			return -EFAULT;

		poke_mem(tracee, peek_reg(tracee, ORIGINAL, SYSARG_3), 0);
		if (errno != 0)
			return -EFAULT;

		/* Force success.  */
		poke_reg(tracee, SYSARG_RESULT, 0);
		return 0;

	case PR_fstatat64:
	case PR_newfstatat:
	case PR_stat64:
	case PR_lstat64:
	case PR_fstat64:
	case PR_stat:
	case PR_lstat:
	case PR_fstat: {
		word_t result;
		word_t address;
		word_t uid, gid;
		Reg sysarg;

		/* Override only if it succeed.  */
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		if (result != 0)
			return 0;

		/* Get the address of the 'stat' structure.  */
		if (sysnum == PR_fstatat64 || sysnum == PR_newfstatat)
			sysarg = SYSARG_3;
		else
			sysarg = SYSARG_2;

		address = peek_reg(tracee, ORIGINAL, sysarg);

		/* Get the uid & gid values from the 'stat' structure.  */
		uid = peek_mem(tracee, address + offsetof_stat_uid(tracee));
		if (errno != 0)
			uid = 0; /* Not fatal.  */

		gid = peek_mem(tracee, address + offsetof_stat_gid(tracee));
		if (errno != 0)
			gid = 0; /* Not fatal.  */

		/* These values are 32-bit width, even on 64-bit architecture.  */
		uid &= 0xFFFFFFFF;
		gid &= 0xFFFFFFFF;

		/* Override only if the file is owned by the current user.
		 * Errors are not fatal here.  */
		if (uid == getuid())
			poke_mem(tracee, address + offsetof_stat_uid(tracee), 0);
		if (gid == getgid())
			poke_mem(tracee, address + offsetof_stat_gid(tracee), 0);

		return 0;
	}

	case PR_getuid:
	case PR_getgid:
	case PR_getegid:
	case PR_geteuid:
	case PR_getuid32:
	case PR_getgid32:
	case PR_geteuid32:
	case PR_getegid32:
	case PR_setuid:
	case PR_setgid:
	case PR_setfsuid:
	case PR_setfsgid:
	case PR_setuid32:
	case PR_setgid32:
	case PR_setfsuid32:
	case PR_setfsgid32:
		/* Force success.  */
		poke_reg(tracee, SYSARG_RESULT, 0);
		return 0;

	default:
		return 0;
	}
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int fake_id0_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	case INITIALIZATION:
		extension->filtered_sysnums = filtered_sysnums;
		return 0;

	case INHERIT_PARENT: /* Inheritable for sub reconfiguration ...  */
		return 1;

	case INHERIT_CHILD:  /* ... but there's nothing else to do.  */
		return 0;

	case HOST_PATH:
		handle_host_path(TRACEE(extension), (char*) data1, (bool) data2);
		return 0;

	case SYSCALL_EXIT_END:
		return handle_sysexit_end(TRACEE(extension));

	default:
		return 0;
	}
}
