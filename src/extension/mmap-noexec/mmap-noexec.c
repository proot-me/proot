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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "syscall/sysnum.h"
#include "arch.h"
#include "attribute.h"

static bool is_mmap_fd(Tracee *tracee)
{
	word_t sysnum;
	word_t flags;
	word_t prot;

	sysnum = get_sysnum(tracee, ORIGINAL);
	if (sysnum != PR_mmap && sysnum != PR_mmap2)
		return false;

	flags = peek_reg(tracee, ORIGINAL, SYSARG_4);
	if ((flags & MAP_ANONYMOUS) != 0)
		return false;

	prot = peek_reg(tracee, ORIGINAL, SYSARG_3);
	if ((prot & PROT_EXEC) == 0)
		return false;

	return true;
}

/**
 * Handler for this @extension. It is triggered each time an @event
 * occurred. See ExtensionEvent for the meaning of @data1 and @data2.
 */
int mmap_noexec_callback(Extension *extension, ExtensionEvent event,
			intptr_t data1 UNUSED, intptr_t data2 UNUSED)
{
	Tracee *tracee;
	int status;

	switch (event) {
	case INITIALIZATION: {
		/* List of syscalls handled by this extensions. */
		static FilteredSysnum filtered_sysnums[] = {
			{ PR_mmap, FILTER_SYSEXIT },
			{ PR_mmap2, FILTER_SYSEXIT },
			FILTERED_SYSNUM_END,
		};
		extension->filtered_sysnums = filtered_sysnums;
		break;
	}

	case SYSCALL_ENTER_START: {
		tracee = TRACEE(extension);

		if (!is_mmap_fd(tracee))
			break;

		/* Force anonymous mmaping.  */
		poke_reg(tracee, SYSARG_4, peek_reg(tracee, ORIGINAL, SYSARG_4) | MAP_ANONYMOUS);
		poke_reg(tracee, SYSARG_5, -1);

		break;
	}

	case SYSCALL_EXIT_START: {
		word_t result;
		word_t length;
		word_t offset;

		struct stat info;
		void *local_mapping;
		char *path;
		int fd;

		tracee = TRACEE(extension);

		if (!is_mmap_fd(tracee))
			break;

		/* On error, mmap(2) returns -errno: the last 4k is
		 * reserved for this.  */
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		if ((int) result < 0 && (int) result > -4096)
			break;

		path = talloc_asprintf(tracee->ctx, "/proc/%d/fd/%d",
				tracee->pid, (int) peek_reg(tracee, ORIGINAL, SYSARG_5));
		if (path == NULL) {
			poke_reg(tracee, SYSARG_RESULT, -errno);
			break;
		}

		length = peek_reg(tracee, ORIGINAL, SYSARG_2);
		offset = peek_reg(tracee, ORIGINAL, SYSARG_6);

		/* Don't trust the dynamic linker about the maximum size.  */
		status = stat(path, &info);
		if (status < 0) {
			poke_reg(tracee, SYSARG_RESULT, status);
			break;
		}
		if (length > (word_t) info.st_size)
			length = (word_t) info.st_size;

		fd = open(path, O_RDONLY);
		local_mapping = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, offset);
		status = -errno;
		close(fd);
		if (local_mapping == MAP_FAILED) {
			poke_reg(tracee, SYSARG_RESULT, status);
			break;
		}

		status = write_data(tracee, result, local_mapping, length);
		(void) munmap(local_mapping, length);
		if (status < 0) {
			poke_reg(tracee, SYSARG_RESULT, status);
			break;
		}
		break;
	}

	default:
		break;
	}

	return 0;
}

