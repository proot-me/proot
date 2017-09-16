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

#include <stdint.h>        /* intptr_t, */
#include <stdlib.h>        /* strtoul(3), */
#include <linux/version.h> /* KERNEL_VERSION, */
#include <assert.h>        /* assert(3), */
#include <sys/utsname.h>   /* uname(2), utsname, */
#include <string.h>        /* str*(3), memcpy(3), */
#include <talloc.h>        /* talloc_*, */
#include <fcntl.h>         /* AT_*,  */
#include <sys/ptrace.h>    /* linux.git:c0a3a20b  */
#include <errno.h>         /* errno,  */
#include <linux/auxvec.h>  /* AT_,  */
#include <linux/futex.h>   /* FUTEX_PRIVATE_FLAG */
#include <sys/param.h>     /* MIN, */

#include "extension/extension.h"
#include "syscall/seccomp.h"
#include "syscall/sysnum.h"
#include "syscall/chain.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "execve/auxv.h"
#include "cli/note.h"
#include "arch.h"

#include "attribute.h"
#include "compat.h"

#define MAX_ARG_SHIFT 2
typedef struct {
	int expected_release;
	word_t new_sysarg_num;
	struct {
		Reg sysarg;     /* first argument to be moved.  */
		size_t nb_args; /* number of arguments to be moved.  */
		int offset;     /* offset to be applied.  */
	} shifts[MAX_ARG_SHIFT];
} Modif;

#define NONE {{0, 0, 0}}

typedef struct {
	int actual_release;
	int virtual_release;
	struct utsname utsname;
	word_t hwcap;
} Config;

/**
 * Return whether the @expected_release is newer than
 * @config->actual_release and older than @config->virtual_release.
 */
static bool needs_kompat(const Config *config, int expected_release)
{
	return (expected_release > config->actual_release
		&& expected_release <= config->virtual_release);
}

/**
 * Modify the current syscall of @tracee as described by @modif
 * regarding the given @config.  This function returns whether the
 * syscall was modified or not.
 */
static bool modify_syscall(Tracee *tracee, const Config *config, const Modif *modif)
{
	size_t i, j;
	word_t syscall;

	assert(config != NULL);

	if (!needs_kompat(config, modif->expected_release))
		return false;

	/* Check if this syscall is supported on this architecture.  */
	syscall = detranslate_sysnum(get_abi(tracee), modif->new_sysarg_num);
	if (syscall == SYSCALL_AVOIDER)
		return false;

	set_sysnum(tracee, modif->new_sysarg_num);

	/* Shift syscall arguments.  */
	for (i = 0; i < MAX_ARG_SHIFT; i++) {
		Reg sysarg     = modif->shifts[i].sysarg;
		size_t nb_args = modif->shifts[i].nb_args;
		int offset     = modif->shifts[i].offset;

		for (j = 0; j < nb_args; j++) {
			word_t arg = peek_reg(tracee, CURRENT, sysarg + j);
			poke_reg(tracee, sysarg + j + offset, arg);
		}
	}

	return true;
}

/**
 * Return the numeric value for the given kernel @release.
 */
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

/**
 * Remove @discarded_flags from the given @tracee's @sysarg register
 * if the actual kernel release is not compatible with the
 * @expected_release.
 */
static void discard_fd_flags(Tracee *tracee, const Config *config,
			int discarded_flags, int expected_release, Reg sysarg)
{
	word_t flags;

	if (!needs_kompat(config, expected_release))
		return;

	flags = peek_reg(tracee, CURRENT, sysarg);
	poke_reg(tracee, sysarg, flags & ~discarded_flags);
}

/**
 * Replace current @tracee's syscall with an older and compatible one
 * whenever it's required, i.e. when the syscall is supported by the
 * kernel as specified by @config->virtual_release but it isn't
 * supported by the actual kernel.
 */
static int handle_sysenter_end(Tracee *tracee, Config *config)
{
	/* Note: syscalls like "openat" can be replaced by "open" since PRoot
	 * has canonicalized "fd + path" into "path".  */
	switch (get_sysnum(tracee, ORIGINAL)) {
	case PR_accept4: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,28),
			.new_sysarg_num   = PR_accept,
			.shifts		  = NONE
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_dup3: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_dup2,
			.shifts		  = NONE
		};

		/* "If oldfd equals newfd, then dup3() fails with the
		 * error EINVAL" -- man dup3 */
		if (peek_reg(tracee, CURRENT, SYSARG_1) == peek_reg(tracee, CURRENT, SYSARG_2))
			return -EINVAL;

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_epoll_create1: {
		bool modified;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_epoll_create,
			.shifts		  = NONE
		};

		/* "the size argument is ignored, but must be greater
		 * than zero" -- man epoll_create */
		modified = modify_syscall(tracee, config, &modif);
		if (modified)
			poke_reg(tracee, SYSARG_1, 1);
		return 0;
	}

	case PR_epoll_pwait: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,19),
			.new_sysarg_num   = PR_epoll_wait,
			.shifts		  = NONE
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_eventfd2: {
		bool modified;
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_eventfd,
			.shifts		  = NONE
		};

		modified = modify_syscall(tracee, config, &modif);
		if (modified) {
			/* EFD_SEMAPHORE can't be emulated with eventfd.  */
			flags = peek_reg(tracee, CURRENT, SYSARG_2);
			if ((flags & EFD_SEMAPHORE) != 0)
				return -EINVAL;
		}
		return 0;
	}

	case PR_faccessat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_access,
			.shifts	= { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 2,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_fchmodat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_chmod,
			.shifts	= { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 2,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_fchownat: {
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.shifts	= { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 3,
					.offset  = -1 }
			}
		};

		flags = peek_reg(tracee, CURRENT, SYSARG_5);
		modif.new_sysarg_num = ((flags & AT_SYMLINK_NOFOLLOW) != 0
					? PR_lchown
					: PR_chown);

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_fcntl: {
		word_t command;

		if (!needs_kompat(config, KERNEL_VERSION(2,6,24)))
			return 0;

		command = peek_reg(tracee, ORIGINAL, SYSARG_2);
		if (command == F_DUPFD_CLOEXEC)
			poke_reg(tracee, SYSARG_2, F_DUPFD);

		return 0;
	}

	case PR_newfstatat:
	case PR_fstatat64: {
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 2,
					.offset  = -1 }
			}
		};

		flags = peek_reg(tracee, CURRENT, SYSARG_4);
		if ((flags & ~AT_SYMLINK_NOFOLLOW) != 0)
			return -EINVAL; /* Exposed by LTP.  */

#if defined(ARCH_X86_64)
		if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
			modif.new_sysarg_num = (get_abi(tracee) != ABI_2 ? PR_lstat : PR_lstat64);
		else
			modif.new_sysarg_num = (get_abi(tracee) != ABI_2 ? PR_stat : PR_stat64);
#else
		if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
			modif.new_sysarg_num = PR_lstat64;
		else
			modif.new_sysarg_num = PR_stat64;
#endif

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_futex: {
		word_t operation;
		static bool warned = false;

		if (!needs_kompat(config, KERNEL_VERSION(2,6,22)) || config->actual_release == 0)
			return 0;

		operation = peek_reg(tracee, CURRENT, SYSARG_2);
		if ((operation & FUTEX_PRIVATE_FLAG) == 0)
			return 0;

		if (!warned) {
			warned = true;
			note(tracee, WARNING, USER,
				"kompat: this kernel doesn't support private futexes "
				"and PRoot can't emulate them.  Expect some troubles...");
		}

		poke_reg(tracee, SYSARG_2, operation & ~FUTEX_PRIVATE_FLAG);
		return 0;
	}

	case PR_futimesat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_utimes,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 2,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_inotify_init1: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_inotify_init,
			.shifts		  = NONE
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_linkat: {
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_link,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 1,
					.offset  = -1 },
				    [1] = {
					.sysarg  = SYSARG_4,
					.nb_args = 1,
					.offset  = -2 }
			}
		};

		flags = peek_reg(tracee, CURRENT, SYSARG_5);
		if ((flags & ~AT_SYMLINK_FOLLOW) != 0)
			return -EINVAL; /* Exposed by LTP.  */

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_mkdirat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_mkdir,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 2,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_mknodat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_mknod,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 3,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_openat: {
		bool modified;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_open,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 3,
					.offset  = -1 }
			}
		};
		modified = modify_syscall(tracee, config, &modif);
		discard_fd_flags(tracee, config, O_CLOEXEC, KERNEL_VERSION(2,6,23),
				modified ? SYSARG_2 : SYSARG_3);
		return 0;
	}

	case PR_open:
		discard_fd_flags(tracee, config, O_CLOEXEC, KERNEL_VERSION(2,6,23), SYSARG_2);
		return 0;

	case PR_pipe2: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_pipe,
			.shifts		  = NONE
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_pselect6: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.shifts		  = NONE
		};
#if defined(ARCH_X86_64)
		modif.new_sysarg_num = (get_abi(tracee) != ABI_2 ? PR_select : PR__newselect);
#else
		modif.new_sysarg_num = PR__newselect;
#endif

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_readlinkat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_readlink,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 3,
					.offset  = -1}
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_renameat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_rename,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 1,
					.offset  =-1 },
				    [1] = {
					    .sysarg  = SYSARG_4,
					    .nb_args = 1,
					    .offset  = -2 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_signalfd4: {
		bool modified;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_signalfd,
			.shifts		  = NONE
		};

		/* "In Linux up to version 2.6.26, the flags argument
		 * is unused, and must be specified as zero." -- man
		 * signalfd */
		modified = modify_syscall(tracee, config, &modif);
		if (modified)
			poke_reg(tracee, SYSARG_4, 0);
		return 0;
	}

	case PR_socket:
	case PR_socketpair:
	case PR_timerfd_create:
		discard_fd_flags(tracee, config, O_CLOEXEC | O_NONBLOCK,
				KERNEL_VERSION(2,6,27), SYSARG_2);
		return 0;

	case PR_symlinkat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_symlink,
			.shifts = { [0] = {
					.sysarg  = SYSARG_3,
					.nb_args = 1,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return 0;
	}

	case PR_unlinkat: {
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 1,
					.offset  = -1
				}
			}
		};

		flags = peek_reg(tracee, CURRENT, SYSARG_3);
		modif.new_sysarg_num = ((flags & AT_REMOVEDIR) != 0
					? PR_rmdir
					: PR_unlink);

		modify_syscall(tracee, config, &modif);
		return 0;
	}

	default:
		return 0;
	}
}

/**
 * Adjust some ELF auxiliary vectors to improve the compatibility.
 * This function assumes the "argv, envp, auxv" stuff is pointed to by
 * @tracee's stack pointer, as expected right after a successful call
 * to execve(2).
 */
static void adjust_elf_auxv(Tracee *tracee, Config *config)
{
	ElfAuxVector *vectors;
	ElfAuxVector *vector;
	word_t vectors_address;
	word_t stack_pointer;
	void *argv_envp;
	size_t size;
	int status;

	vectors_address = get_elf_aux_vectors_address(tracee);
	if (vectors_address == 0)
		return;

	vectors = fetch_elf_aux_vectors(tracee, vectors_address);
	if (vectors == NULL)
		return;

	for (vector = vectors; vector->type != AT_NULL; vector++) {
		switch (vector->type) {
		/* Discard AT_SYSINFO* vectors: they can be used to
		 * get the OS release number from memory instead of
		 * from the uname syscall, and only this latter is
		 * currently hooked by PRoot.  */
		case AT_SYSINFO_EHDR:
		case AT_SYSINFO:
			vector->type  = AT_IGNORE;
			vector->value = 0;
			break;

		case AT_HWCAP:
			if (config->hwcap != (word_t) -1)
				vector->value = config->hwcap;
			break;

		case AT_RANDOM:
			/* Skip only if not in forced mode.  */
			if (config->actual_release != 0)
				goto end;
			break;

		default:
			break;
		}
	}

	/* Add the AT_RANDOM vector only if needed.  */
	if (!needs_kompat(config, KERNEL_VERSION(2,6,29)))
		goto end;

	status = add_elf_aux_vector(&vectors, AT_RANDOM, vectors_address);
	if (status < 0)
		goto end; /* Not fatal.  */

	/* Since a new vector needs to be added, the ELF auxiliary
	 * vectors array can't be pushed in place.  As a consequence,
	 * argv[] and envp[] arrays are moved one vector downward to
	 * make room for the new ELF auxiliary vectors array.
	 * Remember, the stack layout is as follow right after execve:
	 *
	 *     argv[], envp[], auxv[]
	 */
	stack_pointer = peek_reg(tracee, CURRENT, STACK_POINTER);
	size = vectors_address - stack_pointer;
	argv_envp = talloc_size(tracee->ctx, size);
	if (argv_envp == NULL)
		goto end;

	status = read_data(tracee, argv_envp, stack_pointer, size);
	if (status < 0)
		goto end;

	/* Allocate enough room in tracee's stack for the new ELF
	 * auxiliary vector.  */
	stack_pointer   -= 2 * sizeof_word(tracee);
	vectors_address -= 2 * sizeof_word(tracee);

	/* Note that it is safe to update the stack pointer manually
	 * since we are in execve sysexit.  However it should be done
	 * before transfering data since the kernel might not allow
	 * page faults below the stack pointer.  */
	poke_reg(tracee, STACK_POINTER, stack_pointer);

	status = write_data(tracee, stack_pointer, argv_envp, size);
	if (status < 0)
		return;

end:
	push_elf_aux_vectors(tracee, vectors, vectors_address);
	return;
}

/**
 * Append to the @tracee's current syscall enough calls to fcntl(@fd)
 * in order to set the flags from the original @sysarg register, if
 * there are also set in @emulated_flags.
 */
static void emulate_fd_flags(Tracee *tracee, word_t fd, Reg sysarg, int emulated_flags)
{
	word_t flags;

	flags = peek_reg(tracee, ORIGINAL, sysarg);
	if (flags == 0)
		return;

	if ((emulated_flags & flags & O_CLOEXEC) != 0)
		register_chained_syscall(tracee, PR_fcntl, fd, F_SETFD, FD_CLOEXEC, 0, 0, 0);

	if ((emulated_flags & flags & O_NONBLOCK) != 0)
		register_chained_syscall(tracee, PR_fcntl, fd, F_SETFL, O_NONBLOCK, 0, 0, 0);

	force_chain_final_result(tracee, peek_reg(tracee, CURRENT, SYSARG_RESULT));
}

/**
 * Adjust the results/output parameters for syscalls that were
 * modified in handle_sysenter_end().  This function returns -errno if
 * an error occured, otherwise 0.
 */
static int handle_sysexit_end(Tracee *tracee, Config *config)
{
	word_t result;
	word_t sysnum;
	int status;

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	sysnum = get_sysnum(tracee, ORIGINAL);

	/* Error reported by the kernel.  */
	status = (int) result;
	if (status < 0)
		return 0;

	switch (sysnum) {
	case PR_uname: {
		word_t address;

		address = peek_reg(tracee, ORIGINAL, SYSARG_1);

		/* The layout of struct utsname does not depend on the
		 * architecture, it only depends on the kernel
		 * version.  In this regards, this structure is stable
		 * since < 2.6.0.  */
		status = write_data(tracee, address, &config->utsname, sizeof(config->utsname));
		if (status < 0)
			return status;
		return 0;
	}

	case PR_setdomainname:
	case PR_sethostname: {
		word_t address;
		word_t length;
		char *name;

		name = (sysnum == PR_setdomainname
			? config->utsname.domainname
			: config->utsname.nodename);

		length = peek_reg(tracee, ORIGINAL, SYSARG_2);
		if (length > sizeof(config->utsname.domainname) - 1)
			return -EINVAL;

		/* Because of the test above.  */
		assert(sizeof(config->utsname.domainname) == sizeof(config->utsname.nodename));

		address = peek_reg(tracee, ORIGINAL, SYSARG_1);
		status  = read_data(tracee, name, address, length);
		if (status < 0)
			return status;

		/* "name does not require a terminating null byte." --
		 * man 2 set{domain,host}name.  */
		name[length] = '\0';

		return 0;
	}

	case PR_accept4:
		if (get_sysnum(tracee, MODIFIED) == PR_accept)
			emulate_fd_flags(tracee, result, SYSARG_4, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_dup3:
		if (get_sysnum(tracee, MODIFIED) == PR_dup2)
			emulate_fd_flags(tracee, peek_reg(tracee, ORIGINAL, SYSARG_2),
					SYSARG_3, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_epoll_create1:
		if (get_sysnum(tracee, MODIFIED) == PR_epoll_create)
			emulate_fd_flags(tracee, result, SYSARG_1, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_eventfd2:
		if (get_sysnum(tracee, MODIFIED) == PR_eventfd)
			emulate_fd_flags(tracee, result, SYSARG_2, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_fcntl: {
		word_t command;

		if (!needs_kompat(config, KERNEL_VERSION(2,6,24)))
			return 0;

		command = peek_reg(tracee, ORIGINAL, SYSARG_2);
		if (command != F_DUPFD_CLOEXEC)
			return 0;

		register_chained_syscall(tracee, PR_fcntl, result, F_SETFD, FD_CLOEXEC, 0, 0, 0);
		force_chain_final_result(tracee, peek_reg(tracee, CURRENT, SYSARG_RESULT));
		return 0;
	}

	case PR_inotify_init1:
		if (get_sysnum(tracee, MODIFIED) == PR_inotify_init)
			emulate_fd_flags(tracee, result, SYSARG_1, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_open:
		if (needs_kompat(config, KERNEL_VERSION(2,6,23)))
			emulate_fd_flags(tracee, result, SYSARG_2, O_CLOEXEC);
		return 0;

	case PR_openat:
		if (needs_kompat(config, KERNEL_VERSION(2,6,23)))
			emulate_fd_flags(tracee, result, SYSARG_3, O_CLOEXEC);
		return 0;

	case PR_pipe2: {
		int fds[2];

		if (get_sysnum(tracee, MODIFIED) != PR_pipe)
			return 0;

		status = read_data(tracee, fds, peek_reg(tracee, MODIFIED, SYSARG_1), sizeof(fds));
		if (status < 0)
			return 0;

		emulate_fd_flags(tracee, fds[0], SYSARG_2, O_CLOEXEC | O_NONBLOCK);
		emulate_fd_flags(tracee, fds[1], SYSARG_2, O_CLOEXEC | O_NONBLOCK);

		return 0;
	}

	case PR_signalfd4:
		if (get_sysnum(tracee, MODIFIED) == PR_signalfd)
			emulate_fd_flags(tracee, result, SYSARG_4, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_socket:
	case PR_timerfd_create:
		if (needs_kompat(config, KERNEL_VERSION(2,6,27)))
			emulate_fd_flags(tracee, result, SYSARG_2, O_CLOEXEC | O_NONBLOCK);
		return 0;

	case PR_socketpair: {
		int fds[2];

		if (!needs_kompat(config, KERNEL_VERSION(2,6,27)))
			return 0;

		status = read_data(tracee, fds, peek_reg(tracee, MODIFIED, SYSARG_4), sizeof(fds));
		if (status < 0)
			return 0;

		emulate_fd_flags(tracee, fds[0], SYSARG_2, O_CLOEXEC | O_NONBLOCK);
		emulate_fd_flags(tracee, fds[1], SYSARG_2, O_CLOEXEC | O_NONBLOCK);

		return 0;
	}

	default:
		return 0;
	}

	return 0;
}

/**
 * Fill @config->utsname and @config->hwcap according to the content
 * of @string.  This function returns -1 if there is a parsing error,
 * otherwise 0.
 */
static int parse_utsname(Config *config, const char *string)
{
	struct utsname utsname;
	int status;

	assert(string != NULL);

	status = uname(&utsname);
	if (status < 0 || getenv("PROOT_FORCE_KOMPAT") != NULL)
		config->actual_release = 0;
	else
		config->actual_release = parse_kernel_release(utsname.release);

	/* Check whether it is the simple format (ie. release number),
	 * or the complex one:
	 *
	 *     '\sysname\nodename\release\version\machine\domainname\hwcap\'
	 *
	 * This complex format is ugly on purpose: it ain't to be used
	 * directly by users.  */
	if (string[0] == '\\') {
		const char *start;
		const char *end;
		char *end2;

		/* Initial state of the parser.  */
		end = string;

#define PARSE(field) do {						\
			size_t length;					\
									\
			start = end + 1;				\
			end   = strchr(start, '\\');			\
			if (end == NULL) {				\
				note(NULL, ERROR, USER,			\
					"can't find %s field in '%s'", #field, string);	\
				return -1;				\
			}						\
									\
			length = end - start;				\
			length = MIN(length, sizeof(config->utsname.field) - 1); \
			strncpy(config->utsname.field, start, length);	\
			config->utsname.field[length] = '\0';		\
		} while(0)

		PARSE(sysname);
		PARSE(nodename);
		PARSE(release);
		PARSE(version);
		PARSE(machine);
		PARSE(domainname);

#undef PARSE

		/* The hwcap field is parsed as an hexadecimal value.  */
		errno = 0;
		config->hwcap = strtol(end + 1, &end2, 16);
		if (errno != 0 || end2[0] != '\\') {
			note(NULL, ERROR, USER, "can't find hwcap field in '%s'", string);
			return -1;
		}
	}
	else {
		size_t length;

		memcpy(&config->utsname, &utsname, sizeof(config->utsname));

		length = MIN(strlen(string), sizeof(config->utsname.release) - 1);
		strncpy(config->utsname.release, string, length);
		config->utsname.release[length] = '\0';

		config->hwcap = (word_t) -1;
	}

	config->virtual_release = parse_kernel_release(config->utsname.release);

	return 0;
}

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_accept4,		FILTER_SYSEXIT },
	{ PR_dup3,		FILTER_SYSEXIT },
	{ PR_epoll_create1,	FILTER_SYSEXIT },
	{ PR_epoll_pwait, 	0 },
	{ PR_eventfd2, 		FILTER_SYSEXIT },
	{ PR_execve, 		FILTER_SYSEXIT },
	{ PR_faccessat, 	0 },
	{ PR_fchmodat, 		0 },
	{ PR_fchownat, 		0 },
	{ PR_fcntl, 		FILTER_SYSEXIT },
	{ PR_fstatat64, 	0 },
	{ PR_futimesat, 	0 },
	{ PR_futex, 		0 },
	{ PR_inotify_init1, 	FILTER_SYSEXIT },
	{ PR_linkat, 		0 },
	{ PR_mkdirat, 		0 },
	{ PR_mknodat, 		0 },
	{ PR_newfstatat, 	0 },
	{ PR_open, 		FILTER_SYSEXIT },
	{ PR_openat, 		FILTER_SYSEXIT },
	{ PR_pipe2, 		FILTER_SYSEXIT },
	{ PR_pselect6, 		0 },
	{ PR_readlinkat, 	0 },
	{ PR_renameat, 		0 },
	{ PR_setdomainname,	FILTER_SYSEXIT },
	{ PR_sethostname,	FILTER_SYSEXIT },
	{ PR_signalfd4, 	FILTER_SYSEXIT },
	{ PR_socket,		FILTER_SYSEXIT },
	{ PR_socketpair,	FILTER_SYSEXIT },
	{ PR_symlinkat, 	0 },
	{ PR_timerfd_create,	FILTER_SYSEXIT },
	{ PR_uname, 		FILTER_SYSEXIT },
	{ PR_unlinkat, 		0 },
	FILTERED_SYSNUM_END,
};

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occured.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int kompat_callback(Extension *extension, ExtensionEvent event,
		intptr_t data1, intptr_t data2 UNUSED)
{
	int status;

	switch (event) {
	case INITIALIZATION: {
		Config *config;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		config = extension->config;

		status = parse_utsname(config, (const char *) data1);
		if (status < 0)
			return -1;

		extension->filtered_sysnums = filtered_sysnums;
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		/* Nothing to do if this syscall is being discarded
		 * (because of an error detected by PRoot).  */
		if ((int) data1 < 0)
			return 0;

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
		word_t result = peek_reg(tracee, CURRENT, SYSARG_RESULT);;
		word_t sysnum = get_sysnum(tracee, ORIGINAL);

		/* Note: this can be done only before PRoot pushes the
		 * load script into tracee's stack.  */
		if ((int) result >= 0 && sysnum == PR_execve)
			adjust_elf_auxv(tracee, config);
		return 0;
	}

	default:
		return 0;
	}
}
