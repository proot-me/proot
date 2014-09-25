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
#include <sys/mman.h>   /* MAP_*, */
#include <assert.h>     /* assert(3), */
#include <sys/types.h>  /* kill(2), */
#include <signal.h>     /* kill(2), */
#include <string.h>     /* strerror(3), */
#include <strings.h>    /* bzero(3), */

#include "execve/load.h"
#include "execve/auxv.h"
#include "execve/elf.h"
#include "syscall/sysnum.h"
#include "syscall/syscall.h"
#include "syscall/chain.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "cli/notice.h"

/**
 * Start the loading of @tracee.  This function returns -errno if an
 * error occured, otherwise 0.
 */
int translate_execve_exit(Tracee *tracee)
{
	word_t instr_pointer;
	word_t syscall_result;
	int status;

	syscall_result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) syscall_result < 0)
		return 0;

	/* New processes have no heap.  */
	bzero(tracee->heap, sizeof(Heap));

	/* Adjust ELF auxiliary vectors before saving current
	 * registers since the stack pointer migth be changed.  */
	adjust_elf_aux_vectors(tracee);

	/* Once the loading process is done, registers must be
	 * restored in the same state as they are at the beginning of
	 * execve sysexit.  */
	save_current_regs(tracee, ORIGINAL);

	/* Compute the address of the syscall trap instruction (the
	 * current execve trap does not exist anymore since tracee's
	 * memory was recreated).  The code of the exec-stub on x86_64
	 * is as follow:
	 *
	 * 0000000000400078 <_start>:
	 *   400078:       48 c7 c7 b6 00 00 00    mov    $0xb6,%rdi
	 *   40007f:       48 c7 c0 3c 00 00 00    mov    $0x3c,%rax
	 *   400086:       0f 05                   syscall
	 */
	instr_pointer = peek_reg(tracee, CURRENT, INSTR_POINTER);
	poke_reg(tracee, INSTR_POINTER, instr_pointer + 16);

	/* The loading is performed hijacking dummy syscalls
	 * (PR_execve here, but this could be any syscalls that have
	 * the FILTER_SYSEXIT seccomp flag).  */
	status = register_chained_syscall(tracee, PR_execve, 0, 0, 0, 0, 0, 0);
	if (status < 0)
		return status;

	/* Initial state for the loading process.  */
	tracee->loading.step = LOADING_STEP_OPEN;
	tracee->loading.info = tracee->load_info;

	return 0;
}

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
		status = set_sysarg_path(tracee, tracee->loading.info->path, SYSARG_1);
		if (status < 0) {
			notice(tracee, ERROR, INTERNAL, "can't open '%s': %s",
				       tracee->loading.info->path, strerror(-status));
			goto error;
		}
		break;

	case LOADING_STEP_MMAP:
		i = tracee->loading.index;

		set_sysnum(tracee, PR_mmap);
		poke_reg(tracee, SYSARG_1, tracee->loading.info->mappings[i].addr);
		poke_reg(tracee, SYSARG_2, tracee->loading.info->mappings[i].length);
		poke_reg(tracee, SYSARG_3, tracee->loading.info->mappings[i].prot);
		poke_reg(tracee, SYSARG_4, tracee->loading.info->mappings[i].flags);
		poke_reg(tracee, SYSARG_5, tracee->loading.info->mappings[i].fd);
		poke_reg(tracee, SYSARG_6, tracee->loading.info->mappings[i].offset);
		break;

	case LOADING_STEP_CLOSE:
		set_sysnum(tracee, PR_close);
		poke_reg(tracee, SYSARG_1, tracee->loading.info->mappings[0].fd);
		break;

	default:
		assert(0);
	}

	return;

error:
	notice(tracee, ERROR, USER, "can't load '%s' (killing pid %d)", tracee->exe, tracee->pid);
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
	size_t nb_mappings;
	size_t i;

	switch (tracee->loading.step) {
	case LOADING_STEP_OPEN:
		if (signed_result < 0) {
			notice(tracee, ERROR, INTERNAL, "can't open '%s': %s",
				       tracee->loading.info->path, strerror(-signed_result));
			goto error;
		}

		/* Now the file descriptor is known, then adjust
		 * mappings according to.  */
		nb_mappings = talloc_array_length(tracee->loading.info->mappings);
		for (i = 0; i < nb_mappings; i++) {
			if ((tracee->loading.info->mappings[i].flags & MAP_ANONYMOUS) == 0)
				tracee->loading.info->mappings[i].fd = signed_result;
		}

		tracee->loading.step  = LOADING_STEP_MMAP;
		tracee->loading.index = 0;
		break;

	case LOADING_STEP_MMAP: {
		Mapping *current_mapping;

		current_mapping = &tracee->loading.info->mappings[tracee->loading.index];

		if (   signed_result < 0
		    && signed_result > -4096) {
			notice(tracee, ERROR, INTERNAL, "can't map '%s': %s",
				       tracee->loading.info->path, strerror(-signed_result));
			goto error;
		}

		if (   current_mapping->addr != result
		    && (current_mapping->flags & MAP_FIXED) != 0) {
			notice(tracee, ERROR, INTERNAL, "can't map '%s' to the specified address",
				       tracee->loading.info->path);
			goto error;
		}

		if (current_mapping->clear_length > 0) {
			word_t address = result
					+ current_mapping->length
					- current_mapping->clear_length;
			status = clear_mem(tracee, address, current_mapping->clear_length);
		}

		nb_mappings = talloc_array_length(tracee->loading.info->mappings);
		tracee->loading.index++;

		if (tracee->loading.index >= nb_mappings)
			tracee->loading.step  = LOADING_STEP_CLOSE;
		break;
	}

	case LOADING_STEP_CLOSE:
		if (signed_result < 0) {
			notice(tracee, ERROR, INTERNAL, "can't close '%s': %s",
				       tracee->loading.info->path, strerror(-signed_result));
			goto error;
		}

		/* Is this the end of the loading process?  */
		if (tracee->loading.info->interp == NULL) {
			word_t entry_point = ELF_FIELD(tracee->loading.info->elf_header, entry);

			/* Make the process jump to the entry point.  */
			poke_reg(tracee, INSTR_POINTER, entry_point);

			tracee->loading.step = 0;
			return;
		}

		tracee->loading.step = LOADING_STEP_OPEN;
		tracee->loading.info = tracee->load_info->interp;
		break;

	default:
		assert(0);
	}

	/* Chain the next dummy syscall.  */
	status = register_chained_syscall(tracee, PR_execve, 0, 0, 0, 0, 0, 0);
	if (status < 0) {
		notice(tracee, ERROR, INTERNAL, "can't chain dummy syscall");
		goto error;
	}

	return;

error:
	notice(tracee, ERROR, USER, "can't load '%s' (killing pid %d)", tracee->exe, tracee->pid);
	kill(tracee->pid, SIGKILL);
}
