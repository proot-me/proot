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

#include <unistd.h>     /* sysconf(3), _SC*, */
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* PROT_*, */
#include <assert.h>     /* assert(3), */
#include <sys/types.h>  /* kill(2), */
#include <signal.h>     /* kill(2), */
#include <string.h>     /* strerror(3), */

#include "execve/load.h"
#include "execve/elf.h"
#include "syscall/sysnum.h"
#include "syscall/syscall.h"
#include "syscall/chain.h"
#include "tracee/reg.h"
#include "cli/notice.h"

/**
 * Convert the dummy syscall into a useful one according to the
 * @tracee->loading state.  Note that no decision is taken in this
 * function.
 */
void translate_load_enter(Tracee *tracee)
{
	int status;
	size_t i;

	switch (tracee->loading.step) {
	case LOADING_STEP_OPEN:
		set_sysnum(tracee, PR_open);
		status = set_sysarg_path(tracee, tracee->loading.map->path, SYSARG_1);
		if (status < 0) {
			notice(tracee, WARNING, INTERNAL, "can't open '%s': %s",
				       tracee->loading.map->path, strerror(-status));
			goto error;
		}
		break;

	case LOADING_STEP_MMAP:
		i = tracee->loading.index;

		set_sysnum(tracee, PR_mmap);
		poke_reg(tracee, SYSARG_1, tracee->loading.map->mappings[i].addr);
		poke_reg(tracee, SYSARG_2, tracee->loading.map->mappings[i].length);
		poke_reg(tracee, SYSARG_3, tracee->loading.map->mappings[i].prot);
		poke_reg(tracee, SYSARG_4, tracee->loading.map->mappings[i].flags);
		poke_reg(tracee, SYSARG_5, tracee->loading.map->mappings[i].fd);
		poke_reg(tracee, SYSARG_6, tracee->loading.map->mappings[i].offset);
		break;

	case LOADING_STEP_CLOSE:
		set_sysnum(tracee, PR_close);
		poke_reg(tracee, SYSARG_1, tracee->loading.map->mappings[0].fd);
		break;

	default:
		assert(0);
	}

	return;

error:
	notice(tracee, INFO, USER, "can't load '%s' (killing %d)", tracee->exe, tracee->pid);
	kill(tracee->pid, SIGKILL);
}

/**
 * Check the converted dummy syscall succeed (otherwise it kills the
 * @tracee), then prepare the next dummy syscall.
 */
void translate_load_exit(Tracee *tracee)
{
	word_t result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	int signed_result = (int) result;
	int status;
	size_t size;
	size_t i;

	switch (tracee->loading.step) {
	case LOADING_STEP_OPEN:
		if (signed_result < 0) {
			notice(tracee, WARNING, INTERNAL, "can't open '%s': %s",
				       tracee->loading.map->path, strerror(-signed_result));
			goto error;
		}

		/* Now the file descriptor is known, then adjust
		 * mappings according to.  */
		size = talloc_array_length(tracee->loading.map->mappings);
		for (i = 0; i < size; i++) {
			if ((tracee->loading.map->mappings[i].flags & MAP_ANONYMOUS) == 0)
				tracee->loading.map->mappings[i].fd = signed_result;
		}

		tracee->loading.step  = LOADING_STEP_MMAP;
		tracee->loading.index = 0;
		break;

	case LOADING_STEP_MMAP: {
		if (signed_result < 0 && signed_result > -4096) {
			notice(tracee, WARNING, INTERNAL, "can't map '%s': %s",
				       tracee->loading.map->path, strerror(-signed_result));
			goto error;
		}

		if (   tracee->loading.map->mappings[tracee->loading.index].addr != 0
		    && tracee->loading.map->mappings[tracee->loading.index].addr != result) {
			notice(tracee, WARNING, INTERNAL, "can't map '%s' to the specified address",
				       tracee->loading.map->path);
			goto error;
		}

		size = talloc_array_length(tracee->loading.map->mappings);

		/* Now the base address is known, then adjust mappings
		 * according to.  */
		if (tracee->loading.index == 0 && tracee->loading.map->position_independent) {
			for (i = 0; i < size; i++)
				tracee->loading.map->mappings[i].addr += result;
		}

		tracee->loading.index++;
		if (tracee->loading.index >= size)
			tracee->loading.step  = LOADING_STEP_CLOSE;
		break;
	}

	case LOADING_STEP_CLOSE:
		if (signed_result < 0) {
			notice(tracee, WARNING, INTERNAL, "can't close '%s': %s",
				       tracee->loading.map->path, strerror(-signed_result));
			goto error;
		}

		/* Is this the end of the loading process?  */
		if (tracee->loading.map->interp == NULL) {
			tracee->loading.step = 0;
			return;
		}

		tracee->loading.step = LOADING_STEP_OPEN;
		tracee->loading.map  = tracee->load->interp;
		break;

	default:
		assert(0);
	}

	/* Chain the next dummy syscall.  */
	status = register_chained_syscall(tracee, PR_execve, 0, 0, 0, 0, 0, 0);
	if (status < 0) {
		notice(tracee, WARNING, INTERNAL, "can't chain dummy syscall");
		goto error;
	}

	return;

error:
	notice(tracee, INFO, USER, "can't load '%s' (killing %d)", tracee->exe, tracee->pid);
	kill(tracee->pid, SIGKILL);
}
