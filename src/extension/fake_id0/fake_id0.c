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
#if defined(ARCH_X86_64)
static FilteredSyscall syscalls64[] = {
	#include SYSNUM_HEADER
	#include "extension/fake_id0/filter.h"
	#include SYSNUM_HEADER3
	#include "extension/fake_id0/filter.h"
	FILTERED_SYSCALL_END
};

static FilteredSyscall syscalls32[] = {
	#include SYSNUM_HEADER2
	#include "extension/fake_id0/filter.h"
	FILTERED_SYSCALL_END
};

static const Filter filters[] = {
	{ .arch     = AUDIT_ARCH_X86_64,
	  .syscalls = syscalls64 },
	{ .arch     = AUDIT_ARCH_I386,
	  .syscalls = syscalls32 },
	{ 0 }
};
#elif defined(AUDIT_ARCH_NUM)
static FilteredSyscall syscalls[] = {
	#include SYSNUM_HEADER
	#include "extension/fake_id0/filter.h"
	FILTERED_SYSCALL_END
};

static const Filter filters[] = {
	{ .arch     = AUDIT_ARCH_NUM,
	  .syscalls = syscalls },
	{ 0 }
};
#else
static const Filter filters[] = { 0 };
#endif
#include "syscall/sysnum-undefined.h"

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int fake_id0_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	case INITIALIZATION:
		extension->filters = filters;
		return 0;

	case INHERIT_PARENT: /* Inheritable for sub reconfiguration ...  */
		return 1;

	case INHERIT_CHILD:  /* ... but there's nothing else to do.  */
		return 0;

	case HOST_PATH: {
		Tracee *tracee = TRACEE(extension);

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/fake_id0/host_path.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/fake_id0/host_path.c"
			return 0;
		}
		#endif
		#ifdef SYSNUM_HEADER3
		case ABI_3: {
			#include SYSNUM_HEADER3
			#include "extension/fake_id0/host_path.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
		return 0;
	}

	case SYSCALL_EXIT_END: {
		Tracee *tracee = TRACEE(extension);
		word_t syscall_number;

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/fake_id0/exit.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/fake_id0/exit.c"
			return 0;
		}
		#endif
		#ifdef SYSNUM_HEADER3
		case ABI_3: {
			#include SYSNUM_HEADER3
			#include "extension/fake_id0/exit.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
		return 0;
	}

	default:
		return 0;
	}
}
