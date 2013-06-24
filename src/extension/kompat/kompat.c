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

#include <stdint.h>        /* intptr_t, */
#include <stdlib.h>        /* strtoul(3), */
#include <linux/version.h> /* KERNEL_VERSION, */
#include <assert.h>        /* assert(3), */
#include <sys/utsname.h>   /* uname(2), utsname, */
#include <string.h>        /* strncpy(3), */
#include <talloc.h>        /* talloc_*, */
#include <fcntl.h>         /* AT_*,  */
#include <sys/ptrace.h>    /* linux.git:c0a3a20b  */
#include <linux/audit.h>   /* AUDIT_ARCH_*,  */

#include "extension/extension.h"
#include "syscall/sysnum.h"
#include "syscall/seccomp.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
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

typedef struct {
	const char *release;
	int emulated_release;
	int actual_release;
} Config;

/**
 * Modify the current syscall of @tracee as described by @modif
 * regarding the given @config.
 */
static void modify_syscall(Tracee *tracee, const Config *config, const Modif *modif)
{
	size_t i, j;

	assert(config != NULL);

	/* Emulate only syscalls that are available in the expected
	 * release but that are missing in the actual one.  */
	if (   modif->expected_release <= config->actual_release
	    || modif->expected_release > config->emulated_release)
		return;

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

	return;
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
 * Replace current @tracee's syscall with an older and compatible one
 * whenever it's required, i.e. when the syscall is supported by the
 * kernel as specified by @config->release but it isn't supported by
 * the actual kernel.
 */
static void handle_sysenter_end(Tracee *tracee, Config *config)
{
	/* Note: syscalls like "openat" can be replaced by "open" since PRoot
	 * has canonicalized "fd + path" into "path".  */
	switch (get_sysnum(tracee)) {
	case PR_accept4: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,28),
			.new_sysarg_num   = PR_accept,
			.shifts		  = {{0}}
		};
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_dup3: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_dup2,
			.shifts		  = {{0}}
		};
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_epoll_create1: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_epoll_create,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_1, 0); /* Force "size" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_epoll_pwait: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,19),
			.new_sysarg_num   = PR_epoll_wait,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_5, 0); /* Force "sigmask" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_eventfd2: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_eventfd,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_2, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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

		poke_reg(tracee, SYSARG_5, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		modif.new_sysarg_num = ((flags & AT_SYMLINK_NOFOLLOW) != 0
					? PR_lstat
#if defined(ARCH_X86_64)
					: PR_stat);
#else
		: PR_stat64);
#endif

		poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		return;
	}

	case PR_inotify_init1: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_inotify_init,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_1, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_linkat: {
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
		poke_reg(tracee, SYSARG_5, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		return;
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
		return;
	}

	case PR_openat: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.new_sysarg_num   = PR_open,
			.shifts = { [0] = {
					.sysarg  = SYSARG_2,
					.nb_args = 3,
					.offset  = -1 }
			}
		};
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_pipe2: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,27),
			.new_sysarg_num   = PR_pipe,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_2, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

	case PR_pselect6: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,19),
#if defined(ARCH_X86_64)
			.new_sysarg_num   = PR_select,
#else
			.new_sysarg_num   = PR__newselect,
#endif
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_6, 0); /* Force "sigmask" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
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
		return;
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
		return;
	}

	case PR_signalfd4: {
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,17),
			.new_sysarg_num   = PR_signalfd,
			.shifts		  = {{0}}
		};
		poke_reg(tracee, SYSARG_3, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

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
		return;
	}

	case PR_unlinkat: {
		word_t flags;
		Modif modif = {
			.expected_release = KERNEL_VERSION(2,6,16),
			.shifts = { [0] = {
					.sysarg  =  SYSARG_2,
					.nb_args = 1,
					.offset  = -1
				}
			}
		};

		flags = peek_reg(tracee, CURRENT, SYSARG_3);
		modif.new_sysarg_num = ((flags & AT_REMOVEDIR) != 0
					? PR_rmdir
					: PR_unlink);

		poke_reg(tracee, SYSARG_3, 0); /* Force "flags" to 0.  */
		modify_syscall(tracee, config, &modif);
		return;
	}

	default:
		return;
	}
}

/**
 * Force current @tracee's utsname.release to @config->release .  This
 * function returns -errno if an error occured, otherwise 0.
 */
static int handle_sysexit_end(const Tracee *tracee, Config *config)
{
	struct utsname utsname;
	word_t address;
	word_t result;
	size_t size;
	int status;

	if (get_sysnum(tracee) != PR_uname)
		return 0;

	assert(config->release != NULL);

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0)
		return 0;

	address = peek_reg(tracee, ORIGINAL, SYSARG_1);

	status = read_data(tracee, &utsname, address, sizeof(utsname));
	if (status < 0)
		return status;

	/* Note: on x86_64, we can handle the two modes (32/64) with
	 * the same code since struct utsname has always the same
	 * layout.  */
	size = sizeof(utsname.release);
	strncpy(utsname.release, config->release, size);
	utsname.release[size - 1] = '\0';

	status = write_data(tracee, address, &utsname, sizeof(utsname));
	if (status < 0)
		return status;

	return 0;
}

/* List of syscalls handled by this extensions.  */
static FilteredSyscall filtered_syscalls[] = {
	{ PR_accept4,		0 },
	{ PR_dup3,		0 },
	{ PR_epoll_create1,	0 },
	{ PR_epoll_pwait, 	0 },
	{ PR_eventfd2, 		0 },
	{ PR_faccessat, 	0 },
	{ PR_fchmodat, 		0 },
	{ PR_fchownat, 		0 },
	{ PR_fstatat64, 	0 },
	{ PR_futimesat, 	0 },
	{ PR_inotify_init1, 	0 },
	{ PR_linkat, 		0 },
	{ PR_mkdirat, 		0 },
	{ PR_mknodat, 		0 },
	{ PR_newfstatat, 	0 },
	{ PR_openat, 		0 },
	{ PR_pipe2, 		0 },
	{ PR_pselect6, 		0 },
	{ PR_readlinkat, 	0 },
	{ PR_renameat, 		0 },
	{ PR_signalfd4, 	0 },
	{ PR_symlinkat, 	0 },
	{ PR_uname, 		FILTER_SYSEXIT },
	{ PR_unlinkat, 		0 },
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
		struct utsname utsname;
		Config *config;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		config = extension->config;

		config->release = talloc_strdup(config, (const char *)data1);
		if (config->release == NULL)
			return -1;
		talloc_set_name_const(config->release, "$release");

		config->emulated_release = parse_kernel_release(config->release);

		status = uname(&utsname);
		if (status < 0 || getenv("PROOT_FORCE_SYSCALL_COMPAT") != NULL)
			config->actual_release = 0;
		else
			config->actual_release = parse_kernel_release(utsname.release);

		extension->filtered_syscalls = filtered_syscalls;
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		/* Nothing to do if this syscall is being discarded
		 * (because of an error detected by PRoot).  */
		if ((int) data1 < 0)
			return 0;

		handle_sysenter_end(tracee, config);
		return 0;
	}

	case SYSCALL_EXIT_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		return handle_sysexit_end(tracee, config);
	}

	default:
		return 0;
	}
}
