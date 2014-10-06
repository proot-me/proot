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

#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* MAP_*, */
#include <assert.h>     /* assert(3), */
#include <string.h>     /* strlen(3), */
#include <strings.h>    /* bzero(3), */
#include <errno.h>      /* E*, */

#include "execve/execve.h"
#include "execve/elf.h"
#include "loader/script.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "cli/notice.h"

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
		statement->action = LOAD_ACTION_OPEN_NEXT;
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
int translate_execve_exit(Tracee *tracee)
{
	word_t syscall_result;
	int status;

	syscall_result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) syscall_result < 0)
		return 0;

	/* New processes have no heap.  */
	bzero(tracee->heap, sizeof(Heap));

	/* Transfer the load script to the loader.  */
	status = transfer_load_script(tracee);
	if (status < 0)
		return status; /* Note: it's too late to do anything
				* useful. */

	return 0;
}
