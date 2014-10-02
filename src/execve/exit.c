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

#ifdef EXECVE2

#include <unistd.h>     /* sysconf(3), _SC*, */
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* MAP_*, */
#include <assert.h>     /* assert(3), */
#include <sys/types.h>  /* kill(2), O_*, */
#include <signal.h>     /* kill(2), */
#include <string.h>     /* strerror(3), */
#include <strings.h>    /* bzero(3), */
#include <sys/stat.h>   /* O_*, */
#include <fcntl.h>      /* O_*, */

#include "execve/load.h"
#include "execve/auxv.h"
#include "execve/elf.h"
#include "loader/script.h"
#include "syscall/sysnum.h"
#include "syscall/syscall.h"
#include "syscall/chain.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "cli/notice.h"

#ifdef LOADER2

/**
 * Convert @mappings into load @script statements at the given @cursor
 * position.  This function returns the new cursor position.
 */
static void *transcript_mappings(void *cursor, const Mapping *mappings)
{
	size_t nb_mappings;
	size_t i;

	nb_mappings = talloc_array_length(mappings);
	for (i = 0; i < nb_mappings; i++) {
		LoadStatement *statement = cursor;

		if ((mappings[i].flags & MAP_ANONYMOUS) != 0)
			statement->action = LOAD_ACTION_MMAP_ANON;
		else
			statement->action = LOAD_ACTION_MMAP_FILE;

		statement->mmap.addr   = mappings[i].addr;
		statement->mmap.length = mappings[i].length;
		statement->mmap.prot   = mappings[i].prot;
		statement->mmap.offset = mappings[i].offset;
		statement->mmap.clear_length = mappings[i].clear_length;

		cursor += LOAD_STATEMENT_SIZE(*statement, mmap);
	}

	return cursor;
}

/**
 * Convert @tracee->load_info into a load script, then transfer this
 * latter into @tracee's memory.
 */
static int transfer_load_script(Tracee *tracee)
{
	const word_t stack_pointer = peek_reg(tracee, CURRENT, STACK_POINTER);
	word_t entry_point;

	size_t script_size;
	size_t strings_size;
	size_t string1_size;
	size_t string2_size;
	size_t padding_size;

	word_t string1_address;
	word_t string2_address;

	void *buffer;
	size_t buffer_size;

	LoadStatement *statement;
	void *cursor;
	int status;

	/* Strings addresses are required to generate the load script,
	 * for "open" actions.  Since I want to generate it in one
	 * pass, these strings will be put right below the current
	 * stack pointer -- the only known adresses so far -- in the
	 * "strings area".  */
	string1_size = strlen(tracee->load_info->user_path) + 1;
	string2_size = tracee->load_info->interp == NULL ? 0
		     : strlen(tracee->load_info->interp->user_path) + 1;

	/* A padding will be appended at the end of the load script
	 * (a.k.a "strings area") to ensure this latter is aligned on
	 * a word boundary, for sake of performance.  */
	padding_size = (stack_pointer - string1_size - string2_size) % sizeof(word_t);

	strings_size = string1_size + string2_size + padding_size;
	string1_address = stack_pointer - strings_size;
	string2_address = stack_pointer - strings_size + string1_size;

	/* Compute the size of the load script.  */
	script_size =
		LOAD_STATEMENT_SIZE(*statement, open)
		+ (LOAD_STATEMENT_SIZE(*statement, mmap)
			* talloc_array_length(tracee->load_info->mappings))
		+ (tracee->load_info->interp == NULL ? 0
			: LOAD_STATEMENT_SIZE(*statement, open)
			+ (LOAD_STATEMENT_SIZE(*statement, mmap)
				* talloc_array_length(tracee->load_info->interp->mappings)))
		+ LOAD_STATEMENT_SIZE(*statement, start);

	/* Allocate enough room for both the load script and the
	 * strings area.  */
	buffer_size = script_size + strings_size;
	buffer = talloc_size(tracee->ctx, buffer_size);
	if (buffer == NULL)
		return -ENOMEM;

	cursor = buffer;

	/* Load script statement: open.  */
	statement = cursor;
	statement->action = LOAD_ACTION_OPEN;
	statement->open.string_address = string1_address;

	cursor += LOAD_STATEMENT_SIZE(*statement, open);

	/* Load script statements: mmap.  */
	cursor = transcript_mappings(cursor, tracee->load_info->mappings);

	if (tracee->load_info->interp != NULL) {
		/* Load script statement: open.  */
		statement = cursor;
		statement->action = LOAD_ACTION_OPEN;
		statement->open.string_address = string2_address;

		cursor += LOAD_STATEMENT_SIZE(*statement, open);

		/* Load script statements: mmap.  */
		cursor = transcript_mappings(cursor, tracee->load_info->interp->mappings);

		entry_point = ELF_FIELD(tracee->load_info->interp->elf_header, entry);
	}
	else
		entry_point = ELF_FIELD(tracee->load_info->elf_header, entry);

	/* Load script statement: start.  */
	statement = cursor;
	statement->action = LOAD_ACTION_START;
	statement->start.stack_pointer = stack_pointer;
	statement->start.entry_point   = entry_point;
	statement->start.at_phent = ELF_FIELD(tracee->load_info->elf_header, phentsize);
	statement->start.at_phnum = ELF_FIELD(tracee->load_info->elf_header, phnum);
	statement->start.at_entry = ELF_FIELD(tracee->load_info->elf_header, entry);
	statement->start.at_phdr  = ELF_FIELD(tracee->load_info->elf_header, phoff)
				  + tracee->load_info->mappings[0].addr;

	cursor += LOAD_STATEMENT_SIZE(*statement, start);

	/* Sanity check.  */
	assert((uintptr_t) cursor - (uintptr_t) buffer == script_size);

	/* Convert the load script to the expected format.  */
	if (sizeof_word(tracee) == 4) {
		assert(0); /* TODO.  */
	}

	/* Concatenate the load script and the strings.  */
	memcpy(cursor, tracee->load_info->user_path, string1_size);
	cursor += string1_size;

	if (string2_size != 0) {
		memcpy(cursor, tracee->load_info->interp->user_path, string2_size);
		cursor += string2_size;
	}

	/* Sanity check.  */
	cursor += padding_size;
	assert((uintptr_t) cursor - (uintptr_t) buffer == buffer_size);

	/* Copy everything in the tracee's memory at once.  */
	status = write_data(tracee, stack_pointer - buffer_size, buffer, buffer_size);
	if (status < 0)
		return status;

	/* Update the stack pointer and the pointer to the load
	 * script.  */
	poke_reg(tracee, STACK_POINTER, stack_pointer - buffer_size);
	poke_reg(tracee, SYSARG_1, stack_pointer - buffer_size);

	/* Tracee's stack content is now as follow:
	 *
	 *   +------------+ <- initial stack pointer (higher address)
	 *   |  padding   |
	 *   +------------+
	 *   |  string2   |
	 *   +------------+
	 *   |  string1   |
	 *   +------------+
	 *   |   start    |
	 *   +------------+
	 *   | mmap anon  |
	 *   +------------+
	 *   | mmap file  |
	 *   +------------+
	 *   | open next  |
	 *   +------------+
	 *   | mmap anon. |
	 *   +------------+
	 *   | mmap file  |
	 *   +------------+
	 *   |   open     |
	 *   +------------+ <- stack pointer, sysarg1 (word aligned)
	 */

	/* Remember we are in the sysexit stage, so be sure the
	 * current register values will be used as at the end.  */
	save_current_regs(tracee, ORIGINAL);
	tracee->_regs_were_changed = true;

	return 0;
}

/**
 * Start the loading of @tracee.  This function returns -errno if an
 * error occured, otherwise 0.
 */
int translate_execve_exit2(Tracee *tracee)
{
	word_t syscall_result;
	int status;

	syscall_result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) syscall_result < 0)
		return 0;

	/* New processes have no heap.  */
	bzero(tracee->heap, sizeof(Heap));

	/* Adjust ELF auxiliary vectors before transfering the load
	 * script since the stack pointer might be changed.  */
	//adjust_elf_aux_vectors(tracee);

	/* Transfer the load script to the loader.  */
	status = transfer_load_script(tracee);
	if (status < 0)
		return status; /* Note: it's too late to do anything
				* useful. */

	return 0;
}

#else

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
	 * registers since the stack pointer might be changed.  */
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
	force_chain_final_result(tracee, peek_reg(tracee, CURRENT, SYSARG_RESULT));

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

		status = set_sysarg_path(tracee, tracee->loading.info->host_path, SYSARG_1);
		if (status < 0) {
			notice(tracee, ERROR, INTERNAL, "can't open '%s': %s",
				       tracee->loading.info->host_path, strerror(-status));
			goto error;
		}

		poke_reg(tracee, SYSARG_2, O_RDONLY);
		poke_reg(tracee, SYSARG_3, 0);

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

		/* The end of the loading process is near; its ptracer
		 * -- if any -- can now recieve the sysexit
		 * notification about the underlying execve.  */
		if (tracee->loading.info->interp == NULL)
			tracee->as_ptracee.mask_syscall = false;

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
				       tracee->loading.info->host_path, strerror(-signed_result));
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
				       tracee->loading.info->host_path, strerror(-signed_result));
			goto error;
		}

		if (   current_mapping->addr != result
		    && (current_mapping->flags & MAP_FIXED) != 0) {
			notice(tracee, ERROR, INTERNAL, "can't map '%s' to the specified address",
				       tracee->loading.info->host_path);
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
				       tracee->loading.info->host_path, strerror(-signed_result));
			goto error;
		}

		/* Is this the end of the loading process?  */
		if (tracee->loading.info->interp == NULL) {
			word_t entry_point = ELF_FIELD(tracee->loading.info->elf_header, entry);

			/* Make the process jump to the entry point.  */
			poke_reg(tracee, INSTR_POINTER, entry_point);

			/* If the PTRACE_O_TRACEEXEC option is *not* in effect
			 * for the execing tracee, the kernel delivers an
			 * extra SIGTRAP to the tracee after execve(2)
			 * *returns*.  This is an ordinary signal (similar to
			 * one which can be generated by "kill -TRAP"), not a
			 * special kind of ptrace-stop.  Employing
			 * PTRACE_GETSIGINFO for this signal returns si_code
			 * set to 0 (SI_USER).  This signal may be blocked by
			 * signal mask, and thus may be delivered (much)
			 * later. -- man 2 ptrace
			 *
			 * This signal is delayed so far since the
			 * program was not fully loaded yet; GDB would
			 * get "invalid adress" errors otherwise.  */
			if (    tracee->as_ptracee.ptracer != NULL
			    && (tracee->as_ptracee.options & PTRACE_O_TRACEEXEC) == 0)
				kill(tracee->pid, SIGTRAP);

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

#endif /* LOADER2 */

#endif /* EXECVE2 */
