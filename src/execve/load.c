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
#include <linux/auxvec.h> /* AT_*, */

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
 * Adjust ELF auxiliary vectors for @tracee once it is fully loaded.
 */
static void adjust_elf_aux_vectors(Tracee *tracee)
{
	word_t stack_pointer;
	word_t envp_address;
	word_t auxv_address;
	word_t argv_address;
	word_t argv_length;

	ElfAuxVector *auxv;
	ElfAuxVector *vector;
	word_t data;
	int status;

	/* Right after execve, the stack layout is as follow:
	 *
	 *   +-----------------------+
	 *   | auxv: value = 0       |
	 *   +-----------------------+
	 *   | auxv: type  = AT_NULL |
	 *   +-----------------------+
	 *   | ...                   |
	 *   +-----------------------+
	 *   | auxv: value = ???     |
	 *   +-----------------------+
	 *   | auxv: type  = AT_???  |
	 *   +-----------------------+ <- auxv address
	 *   | envp[j]: NULL         |
	 *   +-----------------------+
	 *   | ...                   |
	 *   +-----------------------+
	 *   | envp[0]: ???          |
	 *   +-----------------------+ <- envp address
	 *   | argv[argc]: NULL      |
	 *   +-----------------------+
	 *   | ...                   |
	 *   +-----------------------+
	 *   | argv[0]: ???          |
	 *   +-----------------------+ <- argc address
	 *   | argc                  |
	 *   +-----------------------+ <- stack pointer
	 */
	stack_pointer = peek_reg(tracee, ORIGINAL, STACK_POINTER);
	argv_length   = peek_word(tracee, stack_pointer) + 1;
	argv_address  = stack_pointer + sizeof_word(tracee);
	envp_address  = argv_address + argv_length * sizeof_word(tracee);
	auxv_address  = envp_address;
	do {
		/* Sadly it is not possible to compute auxv_address
		 * without reading tracee's memory.  */
		data = peek_word(tracee, auxv_address);
		if (errno != 0) {
			notice(tracee, WARNING, INTERNAL, "can't find ELF auxiliary vectors");
			return;
		}
		auxv_address += sizeof_word(tracee);
	} while (data != 0);

	auxv = fetch_elf_aux_vectors(tracee, auxv_address);
	if (auxv == NULL) {
		notice(tracee, WARNING, INTERNAL, "can't read ELF auxiliary vectors");
		return;
	}

	for (vector = auxv; vector->type != AT_NULL; vector++) {
		switch (vector->type) {
		case AT_PHDR:
			vector->value = ELF_FIELD(tracee->load_info->elf_header, phoff)
				+ tracee->load_info->mappings[0].addr;
			break;

		case AT_PHENT:
			vector->value = ELF_FIELD(tracee->load_info->elf_header, phentsize);
			break;

		case AT_PHNUM:
			vector->value = ELF_FIELD(tracee->load_info->elf_header, phnum);
			break;

		case AT_BASE:
			if (tracee->load_info->interp != NULL)
				vector->value = tracee->load_info->interp->mappings[0].addr;
			break;

		case AT_ENTRY:
			vector->value = ELF_FIELD(tracee->load_info->elf_header, entry);

			if (IS_POSITION_INDENPENDANT(tracee->load_info->elf_header))
				vector->value += tracee->load_info->mappings[0].addr;
			break;

		case AT_EXECFN:
			vector->value = peek_word(tracee, argv_address);
			break;

		default:
			break;
		}
	}

	/* TODO: notify extensions */

	status = push_elf_aux_vectors(tracee, auxv, auxv_address);
	if (status < 0) {
		notice(tracee, WARNING, INTERNAL, "can't update ELF auxiliary vectors");
		return;
	}
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
			notice(tracee, ERROR, INTERNAL, "can't open '%s': %s",
				       tracee->loading.info->path, strerror(-signed_result));
			goto error;
		}

		/* Now the file descriptor is known, then adjust
		 * mappings according to.  */
		size = talloc_array_length(tracee->loading.info->mappings);
		for (i = 0; i < size; i++) {
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

		if (   current_mapping->addr != 0
		    && current_mapping->addr != result) {
			notice(tracee, ERROR, INTERNAL, "can't map '%s' to the specified address",
				       tracee->loading.info->path);
			goto error;
		}

		if (current_mapping->clear_length > 0) {
			word_t address = current_mapping->addr
					+ current_mapping->length
					- current_mapping->clear_length;
			status = clear_mem(tracee, address, current_mapping->clear_length);
		}

		size = talloc_array_length(tracee->loading.info->mappings);

		/* Now the base address is known, then adjust
		 * addresses according to.  */
		if (   IS_POSITION_INDENPENDANT(tracee->loading.info->elf_header)
		    && tracee->loading.index == 0) {
			for (i = 0; i < size; i++)
				tracee->loading.info->mappings[i].addr += result;
		}

		tracee->loading.index++;
		if (tracee->loading.index >= size)
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

			/* Adjust the entry point if it is a PIE.  */
			if (IS_POSITION_INDENPENDANT(tracee->loading.info->elf_header))
				entry_point += tracee->loading.info->mappings[0].addr;

			/* Make the process jump to the entry point.  */
			poke_reg(tracee, INSTR_POINTER, entry_point);

			adjust_elf_aux_vectors(tracee);

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
