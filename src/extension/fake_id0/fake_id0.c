/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#include <assert.h>      /* assert(3), */
#include <stdint.h>      /* intptr_t, */
#include <errno.h>       /* E*, */
#include <sys/stat.h>    /* chmod(2), stat(2) */
#include <sys/types.h>   /* uid_t, gid_t, get*id(2), */
#include <unistd.h>      /* get*id(2),  */
#include <sys/ptrace.h>  /* linux.git:c0a3a20b  */
#include <linux/audit.h> /* AUDIT_ARCH_*,  */
#include <string.h>      /* memcpy(3), */
#include <stdlib.h>      /* strtol(3), */
#include <linux/auxvec.h>/* AT_,  */

#include "extension/extension.h"
#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "syscall/seccomp.h"
#include "execve/execve.h"
#include "tracee/tracee.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "execve/auxv.h"
#include "path/binding.h"
#include "arch.h"

typedef struct {
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	uid_t fsuid;

	gid_t rgid;
	gid_t egid;
	gid_t sgid;
	gid_t fsgid;
} Config;

typedef struct {
	char *path;
	mode_t mode;
} ModifiedNode;

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_capset,		FILTER_SYSEXIT },
	{ PR_chmod,		FILTER_SYSEXIT },
	{ PR_chown,		FILTER_SYSEXIT },
	{ PR_chown32,		FILTER_SYSEXIT },
	{ PR_chroot,		FILTER_SYSEXIT },
	{ PR_execve,		FILTER_SYSEXIT },
	{ PR_fchmod,		FILTER_SYSEXIT },
	{ PR_fchmodat,		FILTER_SYSEXIT },
	{ PR_fchown,		FILTER_SYSEXIT },
	{ PR_fchown32,		FILTER_SYSEXIT },
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
	{ PR_getgroups,		FILTER_SYSEXIT },
	{ PR_getgroups32,	FILTER_SYSEXIT },
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
	{ PR_mknod,		FILTER_SYSEXIT },
	{ PR_mknodat,		FILTER_SYSEXIT },
	{ PR_newfstatat,	FILTER_SYSEXIT },
	{ PR_oldlstat,		FILTER_SYSEXIT },
	{ PR_oldstat,		FILTER_SYSEXIT },
	{ PR_setfsgid,		FILTER_SYSEXIT },
	{ PR_setfsgid32,	FILTER_SYSEXIT },
	{ PR_setfsuid,		FILTER_SYSEXIT },
	{ PR_setfsuid32,	FILTER_SYSEXIT },
	{ PR_setgid,		FILTER_SYSEXIT },
	{ PR_setgid32,		FILTER_SYSEXIT },
	{ PR_setgroups,		FILTER_SYSEXIT },
	{ PR_setgroups32,	FILTER_SYSEXIT },
	{ PR_setregid,		FILTER_SYSEXIT },
	{ PR_setregid32,	FILTER_SYSEXIT },
	{ PR_setreuid,		FILTER_SYSEXIT },
	{ PR_setreuid32,	FILTER_SYSEXIT },
	{ PR_setresgid,		FILTER_SYSEXIT },
	{ PR_setresgid32,	FILTER_SYSEXIT },
	{ PR_setresuid,		FILTER_SYSEXIT },
	{ PR_setresuid32,	FILTER_SYSEXIT },
	{ PR_setuid,		FILTER_SYSEXIT },
	{ PR_setuid32,		FILTER_SYSEXIT },
	{ PR_setxattr,		FILTER_SYSEXIT },
	{ PR_setdomainname,	FILTER_SYSEXIT },
	{ PR_sethostname,	FILTER_SYSEXIT },
	{ PR_lsetxattr,		FILTER_SYSEXIT },
	{ PR_fsetxattr,		FILTER_SYSEXIT },
	{ PR_stat,		FILTER_SYSEXIT },
	{ PR_stat64,		FILTER_SYSEXIT },
	{ PR_statfs,		FILTER_SYSEXIT },
	{ PR_statfs64,		FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

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

/**
 * Force permissions of @path to "rwx" during the path translation of
 * current @tracee's syscall, in order to simulate CAP_DAC_OVERRIDE.
 * The original permissions are restored through talloc destructors.
 * See canonicalize() for the meaning of @is_final.
 */
static void override_permissions(const Tracee *tracee, const char *path, bool is_final)
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
 * Adjust current @tracee's syscall parameters according to @config.
 * This function always returns 0.
 */
static int handle_sysenter_end(Tracee *tracee, const Config *config)
{
	word_t sysnum;

	sysnum = get_sysnum(tracee, ORIGINAL);
	switch (sysnum) {
	case PR_setuid:
	case PR_setuid32:
	case PR_setgid:
	case PR_setgid32:
	case PR_setreuid:
	case PR_setreuid32:
	case PR_setregid:
	case PR_setregid32:
	case PR_setresuid:
	case PR_setresuid32:
	case PR_setresgid:
	case PR_setresgid32:
	case PR_setfsuid:
	case PR_setfsuid32:
	case PR_setfsgid:
	case PR_setfsgid32:
		/* These syscalls are fully emulated.  */
		set_sysnum(tracee, PR_void);
		return 0;

	case PR_chown:
	case PR_chown32:
	case PR_lchown:
	case PR_lchown32:
	case PR_fchown:
	case PR_fchown32:
	case PR_fchownat: {
		Reg uid_sysarg;
		Reg gid_sysarg;
		uid_t uid;
		gid_t gid;

		if (sysnum == PR_fchownat) {
			uid_sysarg = SYSARG_3;
			gid_sysarg = SYSARG_4;
		}
		else {
			uid_sysarg = SYSARG_2;
			gid_sysarg = SYSARG_3;
		}

		uid = peek_reg(tracee, ORIGINAL, uid_sysarg);
		gid = peek_reg(tracee, ORIGINAL, gid_sysarg);

		/* Swap actual and emulated ids to get a chance of
		 * success.  */
		if (uid == config->ruid)
			poke_reg(tracee, uid_sysarg, getuid());
		if (gid == config->rgid)
			poke_reg(tracee, gid_sysarg, getgid());

		return 0;
	}

	case PR_setgroups:
	case PR_setgroups32:
	case PR_getgroups:
	case PR_getgroups32:
		/* TODO */

	default:
		return 0;
	}

	/* Never reached  */
	assert(0);
	return 0;

}

/**
 * Copy config->@field to the tracee's memory location pointed to by @sysarg.
 */
#define POKE_MEM_ID(sysarg, field) do {					\
	poke_uint16(tracee, peek_reg(tracee, ORIGINAL, sysarg), config->field);	\
	if (errno != 0)							\
		return -errno;						\
} while (0)

/**
 * Emulate setuid(2) and setgid(2).
 */
#define SETXID(id) do {							\
	id ## _t id = peek_reg(tracee, ORIGINAL, SYSARG_1);		\
	bool allowed;							\
									\
	/* "EPERM: The user is not privileged (does not have the	\
	 * CAP_SETUID capability) and uid does not match the real UID	\
	 * or saved set-user-ID of the calling process." -- man		\
	 * setuid */							\
	allowed = (config->euid == 0 /* TODO: || HAS_CAP(SETUID) */	\
		|| id == config->r ## id				\
		|| id == config->e ## id				\
		|| id == config->s ## id);				\
	if (!allowed)							\
		return -EPERM;						\
									\
	/* "If the effective UID of the caller is root, the real UID	\
	 * and saved set-user-ID are also set." -- man setuid */	\
	if (config->e ## id == 0) {					\
		config->r ## id = id;					\
		config->s ## id = id;					\
	}								\
									\
	/* "whenever the effective user ID is changed, fsuid will also	\
	 * be changed to the new value of the effective user ID."  --	\
	 * man setfsuid */						\
	config->e ## id  = id;						\
	config->fs ## id = id;						\
									\
	poke_reg(tracee, SYSARG_RESULT, 0);				\
	return 0;							\
} while (0)

/**
 * Check whether @id is set or not.
 */
#define UNSET_ID(id) (id == (uid_t) -1)

/**
 * Check whether @id is change or not.
 */
#define UNCHANGED_ID(id) (UNSET_ID(id) || id == config->id)

/**
 * Emulate setreuid(2) and setregid(2).
 */
#define SETREXID(id) do {						\
	id ## _t r ## id = peek_reg(tracee, ORIGINAL, SYSARG_1);	\
	id ## _t e ## id = peek_reg(tracee, ORIGINAL, SYSARG_2);	\
	bool allowed;							\
									\
	/* "Unprivileged processes may only set the effective user ID	\
	 * to the real user ID, the effective user ID, or the saved	\
	 * set-user-ID.							\
	 *								\
	 * Unprivileged users may only set the real user ID to the	\
	 * real user ID or the effective user ID."			\
	 *
	 * "EPERM: The calling process is not privileged (does not	\
	 * have the CAP_SETUID) and a change other than:		\
	 * 1. swapping the effective user ID with the real user ID,	\
	 *    or;							\
	 * 2. setting one to the value of the other, or ;		\
	 * 3. setting the effective user ID to the value of the saved	\
	 *    set-user-ID						\
	 * was specified." -- man setreuid				\
	 *								\
	 * Is it possible to "ruid <- euid" and "euid <- suid" at the	\
	 * same time?  */						\
	allowed = (config->euid == 0 /* TODO: || HAS_CAP(SETUID) */	\
		|| (UNCHANGED_ID(e ## id) && UNCHANGED_ID(r ## id))	\
		|| (r ## id == config->e ## id && (e ## id == config->r ## id || UNCHANGED_ID(e ## id))) \
		|| (e ## id == config->r ## id && (r ## id == config->e ## id || UNCHANGED_ID(r ## id))) \
		|| (e ## id == config->s ## id && UNCHANGED_ID(r ## id))); \
	if (!allowed)							\
		return -EPERM;						\
									\
	/* "Supplying a value of -1 for either the real or effective	\
	 * user ID forces the system to leave that ID unchanged.	\
	 * [...]  If the real user ID is set or the effective user ID	\
	 * is set to a value not equal to the previous real user ID,	\
	 * the saved set-user-ID will be set to the new effective user	\
	 * ID." -- man setreuid */					\
	if (!UNSET_ID(e ## id)) {					\
		if (e ## id != config->r ## id)				\
			config->s ## id = e ## id;			\
									\
		config->e ## id  = e ## id;				\
		config->fs ## id = e ## id;				\
	}								\
									\
	/* Since it changes the current ruid value, this has to be	\
	 * done after euid handling. */					\
	if (!UNSET_ID(r ## id)) {					\
		if (!UNSET_ID(e ## id))					\
			config->s ## id = e ## id;			\
		config->r ## id = r ## id;				\
	}								\
									\
	poke_reg(tracee, SYSARG_RESULT, 0);				\
	return 0;							\
} while (0)

/**
 * Check if @var is equal to any config->r{@type}id's.
 */
#define EQUALS_ANY_ID(var, type)  (var == config->r ## type ## id \
				|| var == config->e ## type ## id \
				|| var == config->s ## type ## id)

/**
 * Emulate setresuid(2) and setresgid(2).
 */
#define SETRESXID(type) do {						\
	type ## id_t r ## type ## id = peek_reg(tracee, ORIGINAL, SYSARG_1);	\
	type ## id_t e ## type ## id = peek_reg(tracee, ORIGINAL, SYSARG_2);	\
	type ## id_t s ## type ## id = peek_reg(tracee, ORIGINAL, SYSARG_3);	\
	bool allowed;							\
									\
	/* "Unprivileged user processes may change the real UID,	\
	 * effective UID, and saved set-user-ID, each to one of: the	\
	 * current real UID, the current effective UID or the current	\
	 * saved set-user-ID.						\
	 *								\
	 * Privileged processes (on Linux, those having the CAP_SETUID	\
	 * capability) may set the real UID, effective UID, and saved	\
	 * set-user-ID to arbitrary values." -- man setresuid */	\
	allowed = (config->euid == 0 /* || HAS_CAP(SETUID) */		\
		|| ((UNSET_ID(r ## type ## id) || EQUALS_ANY_ID(r ## type ## id, type)) \
		 && (UNSET_ID(e ## type ## id) || EQUALS_ANY_ID(e ## type ## id, type)) \
		 && (UNSET_ID(s ## type ## id) || EQUALS_ANY_ID(s ## type ## id, type)))); \
	if (!allowed)							\
		return -EPERM;						\
									\
	/* "If one of the arguments equals -1, the corresponding value	\
	 * is not changed." -- man setresuid */				\
	if (!UNSET_ID(r ## type ## id))					\
		config->r ## type ## id = r ## type ## id;		\
									\
	if (!UNSET_ID(e ## type ## id)) {				\
		/* "the file system UID is always set to the same	\
		 * value as the (possibly new) effective UID." -- man	\
		 * setresuid */						\
		config->e ## type ## id  = e ## type ## id;		\
		config->fs ## type ## id = e ## type ## id;		\
	}								\
									\
	if (!UNSET_ID(s ## type ## id))					\
		config->s ## type ## id = s ## type ## id;		\
									\
	poke_reg(tracee, SYSARG_RESULT, 0);				\
	return 0;							\
} while (0)

/**
 * Emulate setfsuid(2) and setfsgid(2).
 */
#define SETFSXID(type) do {						\
	uid_t fs ## type ## id = peek_reg(tracee, ORIGINAL, SYSARG_1); 	\
	uid_t old_fs ## type ## id = config->fs ## type ## id;		\
	bool allowed;							\
									\
	/* "setfsuid() will succeed only if the caller is the		\
	 * superuser or if fsuid matches either the real user ID,	\
	 * effective user ID, saved set-user-ID, or the current value	\
	 * of fsuid." -- man setfsuid */				\
	allowed = (config->euid == 0 /* TODO: || HAS_CAP(SETUID) */	\
		|| fs ## type ## id == config->fs ## type ## id		\
		|| EQUALS_ANY_ID(fs ## type ## id, type));		\
	if (allowed)							\
		config->fs ## type ## id = fs ## type ## id;		\
									\
	/* "On success, the previous value of fsuid is returned.  On	\
	 * error, the current value of fsuid is returned." -- man	\
	 * setfsuid */							\
	poke_reg(tracee, SYSARG_RESULT, old_fs ## type ## id);		\
	return 0;							\
} while (0)

/**
 * Adjust current @tracee's syscall result according to @config.  This
 * function returns -errno if an error occured, otherwise 0.
 */
static int handle_sysexit_end(Tracee *tracee, Config *config)
{
	word_t sysnum;
	word_t result;

	sysnum = get_sysnum(tracee, ORIGINAL);
	switch (sysnum) {

	case PR_setuid:
	case PR_setuid32:
		SETXID(uid);

	case PR_setgid:
	case PR_setgid32:
		SETXID(gid);

	case PR_setreuid:
	case PR_setreuid32:
		SETREXID(uid);

	case PR_setregid:
	case PR_setregid32:
		SETREXID(gid);

	case PR_setresuid:
	case PR_setresuid32:
		SETRESXID(u);

	case PR_setresgid:
	case PR_setresgid32:
		SETRESXID(g);

	case PR_setfsuid:
	case PR_setfsuid32:
		SETFSXID(u);

	case PR_setfsgid:
	case PR_setfsgid32:
		SETFSXID(g);

	case PR_getuid:
	case PR_getuid32:
		poke_reg(tracee, SYSARG_RESULT, config->ruid);
		return 0;

	case PR_getgid:
	case PR_getgid32:
		poke_reg(tracee, SYSARG_RESULT, config->rgid);
		return 0;

	case PR_geteuid:
	case PR_geteuid32:
		poke_reg(tracee, SYSARG_RESULT, config->euid);
		return 0;

	case PR_getegid:
	case PR_getegid32:
		poke_reg(tracee, SYSARG_RESULT, config->egid);
		return 0;

	case PR_getresuid:
	case PR_getresuid32:
		POKE_MEM_ID(SYSARG_1, ruid);
		POKE_MEM_ID(SYSARG_2, euid);
		POKE_MEM_ID(SYSARG_3, suid);
		return 0;

	case PR_getresgid:
	case PR_getresgid32:
		POKE_MEM_ID(SYSARG_1, rgid);
		POKE_MEM_ID(SYSARG_2, egid);
		POKE_MEM_ID(SYSARG_3, sgid);
		return 0;

	case PR_setdomainname:
	case PR_sethostname:
	case PR_setgroups:
	case PR_setgroups32:
	case PR_mknod:
	case PR_mknodat:
	case PR_capset:
	case PR_setxattr:
	case PR_lsetxattr:
	case PR_fsetxattr:
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

		/* Force success if the tracee was supposed to have
		 * the capability.  */
		if (config->euid == 0) /* TODO: || HAS_CAP(...) */
			poke_reg(tracee, SYSARG_RESULT, 0);

		return 0;
	}

	case PR_fstatat64:
	case PR_newfstatat:
	case PR_stat64:
	case PR_lstat64:
	case PR_fstat64:
	case PR_stat:
	case PR_lstat:
	case PR_fstat: {
		word_t address;
		Reg sysarg;
		uid_t uid;
		gid_t gid;

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

		/* Sanity checks.  */
		assert(__builtin_types_compatible_p(uid_t, uint32_t));
		assert(__builtin_types_compatible_p(gid_t, uint32_t));

		/* Get the uid & gid values from the 'stat' structure.  */
		uid = peek_uint32(tracee, address + offsetof_stat_uid(tracee));
		if (errno != 0)
			uid = 0; /* Not fatal.  */

		gid = peek_uint32(tracee, address + offsetof_stat_gid(tracee));
		if (errno != 0)
			gid = 0; /* Not fatal.  */

		/* Override only if the file is owned by the current user.
		 * Errors are not fatal here.  */
		if (uid == getuid())
			poke_uint32(tracee, address + offsetof_stat_uid(tracee), config->suid);

		if (gid == getgid())
			poke_uint32(tracee, address + offsetof_stat_gid(tracee), config->sgid);

		return 0;
	}

	case PR_chroot: {
		char path[PATH_MAX];
		word_t input;
		int status;

		if (config->euid != 0) /* TODO: && !HAS_CAP(SYS_CHROOT) */
			return 0;

		/* Override only permission errors.  */
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		if ((int) result != -EPERM)
			return 0;

		input = peek_reg(tracee, MODIFIED, SYSARG_1);

		status = read_path(tracee, path, input);
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

	default:
		return 0;
	}
}

#undef POKE_MEM_ID
#undef SETXID
#undef UNSET_ID
#undef UNCHANGED_ID
#undef SETREXID
#undef EQUALS_ANY_ID
#undef SETRESXID
#undef SETFSXID

/**
 * Adjust some ELF auxiliary vectors.  This function assumes the
 * "argv, envp, auxv" stuff is pointed to by @tracee's stack pointer,
 * as expected right after a successful call to execve(2).
 */
static int adjust_elf_auxv(Tracee *tracee, Config *config)
{
	ElfAuxVector *vectors;
	ElfAuxVector *vector;
	word_t vectors_address;

	vectors_address = get_elf_aux_vectors_address(tracee);
	if (vectors_address == 0)
		return 0;

	vectors = fetch_elf_aux_vectors(tracee, vectors_address);
	if (vectors == NULL)
		return 0;

	for (vector = vectors; vector->type != AT_NULL; vector++) {
		switch (vector->type) {
		case AT_UID:
			vector->value = config->ruid;
			break;

		case AT_EUID:
			vector->value = config->euid;
			break;

		case AT_GID:
			vector->value = config->rgid;
			break;

		case AT_EGID:
			vector->value = config->egid;
			break;

		default:
			break;
		}
	}

	push_elf_aux_vectors(tracee, vectors, vectors_address);

	return 0;
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int fake_id0_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	case INITIALIZATION: {
		const char *uid_string = (const char *) data1;
		const char *gid_string;
		Config *config;
		int uid, gid;

		errno = 0;
		uid = strtol(uid_string, NULL, 10);
		if (errno != 0)
			uid = getuid();

		gid_string = strchr(uid_string, ':');
		if (gid_string == NULL) {
			errno = EINVAL;
		}
		else {
			errno = 0;
			gid = strtol(gid_string + 1, NULL, 10);
		}
		/* Fallback to the current gid if an error occured.  */
		if (errno != 0)
			gid = getgid();

		extension->config = talloc(extension, Config);
		if (extension->config == NULL)
			return -1;

		config = talloc_get_type_abort(extension->config, Config);
		config->ruid  = uid;
		config->euid  = uid;
		config->suid  = uid;
		config->fsuid = uid;
		config->rgid  = gid;
		config->egid  = gid;
		config->sgid  = gid;
		config->fsgid = gid;

		extension->filtered_sysnums = filtered_sysnums;
		return 0;
	}

	case INHERIT_PARENT: /* Inheritable for sub reconfiguration ...  */
		return 1;

	case INHERIT_CHILD: {
		/* Copy the parent configuration to the child.  The
		 * structure should not be shared as uid/gid changes
		 * in one process should not affect other processes.
		 * This assertion is not true for POSIX threads
		 * sharing the same group, however Linux threads never
		 * share uid/gid information.  As a consequence, the
		 * GlibC emulates the POSIX behavior on Linux by
		 * sending a signal to all group threads to cause them
		 * to invoke the system call too.  Finally, PRoot
		 * doesn't have to worry about clone flags.
		 */

		Extension *parent = (Extension *) data1;
		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;

		memcpy(extension->config, parent->config, sizeof(Config));
		return 0;
	}

	case HOST_PATH: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		/* Force permissions if the tracee was supposed to
		 * have the capability.  */
		if (config->euid == 0) /* TODO: || HAS_CAP(DAC_OVERRIDE) */
			override_permissions(tracee, (char*) data1, (bool) data2);
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		return handle_sysenter_end(tracee, config);
	}

	case SYSCALL_EXIT_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		return handle_sysexit_end(tracee, config);
	}

	case SYSCALL_EXIT_START: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);
		word_t result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		word_t sysnum = get_sysnum(tracee, ORIGINAL);
		struct stat mode;
		int status;

		if ((int) result < 0 || sysnum != PR_execve)
			return 0;

		/* This has to be done before PRoot pushes the load
		 * script into tracee's stack.  */
		adjust_elf_auxv(tracee, config);

		status = stat(tracee->load_info->host_path, &mode);
		if (status < 0)
			return 0; /* Not fatal.  */

		if ((mode.st_mode & S_ISUID) != 0) {
			config->euid = 0;
			config->suid = 0;
		}

		if ((mode.st_mode & S_ISGID) != 0) {
			config->egid = 0;
			config->sgid = 0;
		}

		return 0;
	}

	default:
		return 0;
	}
}
