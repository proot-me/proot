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

#include <sys/types.h>  /* lstat(2), lseek(2), */
#include <sys/stat.h>   /* lstat(2), lseek(2), */
#include <unistd.h>     /* access(2), lstat(2), close(2), read(2), */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* PROT_*, */
#include <strings.h>    /* bzero(3), */

#include "execve/execve.h"
#include "execve/shebang.h"
#include "execve/load.h"
#include "execve/elf.h"
#include "path/path.h"
#include "tracee/tracee.h"
#include "syscall/chain.h"
#include "syscall/syscall.h"
#include "cli/notice.h"

#define P(a) PROGRAM_FIELD(load_info->elf_header, *program_header, a)

/**
 * Add @program_header (type PT_LOAD) to @load_info->mappings.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_mapping(const Tracee *tracee UNUSED, LoadInfo *load_info,
		const ProgramHeader *program_header)
{
	size_t index;
	word_t start_address;
	word_t end_address;
	static word_t page_size = 0;
	static word_t page_mask = 0;

	if (page_size == 0) {
		page_size = sysconf(_SC_PAGE_SIZE);
		if ((int) page_size <= 0)
			page_size = 0x1000;
		page_mask = ~(page_size - 1);
	}


	if (load_info->mappings == NULL)
		index = 0;
	else
		index = talloc_array_length(load_info->mappings);

	load_info->mappings = talloc_realloc(load_info, load_info->mappings, Mapping, index + 1);
	if (load_info->mappings == NULL)
		return -ENOMEM;

	start_address = P(vaddr) & page_mask;
	end_address   = (P(vaddr) + P(filesz) + page_size) & page_mask;

	load_info->mappings[index].fd     = -1; /* Unknown yet.  */
	load_info->mappings[index].offset = P(offset) & page_mask;
	load_info->mappings[index].addr   = start_address;
	load_info->mappings[index].length = end_address - start_address;
	load_info->mappings[index].flags  = MAP_PRIVATE | MAP_FIXED;
	load_info->mappings[index].prot   =  ( (P(flags) & PF_R ? PROT_READ  : 0)
					| (P(flags) & PF_W ? PROT_WRITE : 0)
					| (P(flags) & PF_X ? PROT_EXEC  : 0));

	/* "If the segment's memory size p_memsz is larger than the
	 * file size p_filesz, the "extra" bytes are defined to hold
	 * the value 0 and to follow the segment's initialized area."
	 * -- man 7 elf.  */
	if (P(memsz) > P(filesz)) {
		/* How many extra bytes in the current page?  */
		load_info->mappings[index].clear_length = end_address - P(vaddr) - P(filesz);

		/* Create new pages for the remaining extra bytes.  */
		start_address = end_address;
		end_address   = (P(vaddr) + P(memsz) + page_size) & page_mask;
		if (end_address > start_address) {
			index++;
			load_info->mappings = talloc_realloc(load_info, load_info->mappings,
							Mapping, index + 1);
			if (load_info->mappings == NULL)
				return -ENOMEM;

			load_info->mappings[index].fd     = -1;  /* Anonymous.  */
			load_info->mappings[index].offset =  0;
			load_info->mappings[index].addr   = start_address;
			load_info->mappings[index].length = end_address - start_address;
			load_info->mappings[index].clear_length = 0;
			load_info->mappings[index].flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
			load_info->mappings[index].prot   = load_info->mappings[index - 1].prot;
		}
	}
	else
		load_info->mappings[index].clear_length = 0;

	return 0;
}

/**
 * Add @program_header (type PT_INTERP) to @load_info->interp.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_interp(Tracee *tracee, int fd, LoadInfo *load_info,
		const ProgramHeader *program_header)
{
	char host_path[PATH_MAX];
	char *user_path;
	int status;

	/* Only one PT_INTERP segment is allowed.  */
	if (load_info->interp != NULL)
		return -EINVAL;

	load_info->interp = talloc_zero(load_info, LoadInfo);
	if (load_info->interp == NULL)
		return -ENOMEM;

	user_path = talloc_size(tracee->ctx, P(filesz) + 1);
	if (user_path == NULL)
		return -ENOMEM;

	/* Remember pread(2) doesn't change the
	 * current position in the file.  */
	status = pread(fd, user_path, P(filesz), P(offset));
	if ((size_t) status != P(filesz)) /* Unexpected size.  */
		status = -EACCES;
	if (status < 0)
		return status;

	user_path[P(filesz)] = '\0';

	status = translate_and_check_exec(tracee, host_path, user_path);
	if (status < 0)
		return status;

	load_info->interp->path = talloc_strdup(load_info->interp, host_path);
	if (load_info->interp->path == NULL)
		return -ENOMEM;

	return 0;
}

#undef P

/**
 * Extract the load info from @load->path.  This function returns
 * -errno if an error occured, otherwise it returns 0.
 *
 * TODO: factorize with find_program_header()
 */
static int extract_load_info(Tracee *tracee, LoadInfo *load_info)
{
	ProgramHeader program_header;

	uint64_t elf_phoff;
	uint16_t elf_phentsize;
	uint16_t elf_phnum;

	int fd = 1;
	int status;
	int i;

	assert(load_info != NULL);
	assert(load_info->path != NULL);

	fd = open_elf(load_info->path, &load_info->elf_header);
	if (fd < 0)
		return fd;

	/* Sanity check.  */
	switch (ELF_FIELD(load_info->elf_header, type)) {
	case ET_EXEC:
	case ET_DYN:
		break;

	default:
		status = -EINVAL;
		goto end;
	}

	/* Get class-specific fields. */
	elf_phnum     = ELF_FIELD(load_info->elf_header, phnum);
	elf_phentsize = ELF_FIELD(load_info->elf_header, phentsize);
	elf_phoff     = ELF_FIELD(load_info->elf_header, phoff);

	/*
	 * Some sanity checks regarding the current
	 * support of the ELF specification in PRoot.
	 */

	if (elf_phnum >= 0xffff) {
		notice(tracee, WARNING, INTERNAL, "big PH tables are not yet supported.");
		status = -ENOTSUP;
		goto end;
	}

	if (!KNOWN_PHENTSIZE(load_info->elf_header, elf_phentsize)) {
		notice(tracee, WARNING, INTERNAL, "unsupported size of program header.");
		status = -ENOTSUP;
		goto end;
	}

	/*
	 * Search the first entry of the requested type into the
	 * program header table.
	 */

	status = (int) lseek(fd, elf_phoff, SEEK_SET);
	if (status < 0) {
		status = -errno;
		goto end;
	}

	for (i = 0; i < elf_phnum; i++) {
		status = read(fd, &program_header, elf_phentsize);
		if (status != elf_phentsize) {
			status = (status < 0 ? -errno : -ENOTSUP);
			goto end;
		}

		switch (PROGRAM_FIELD(load_info->elf_header, program_header, type)) {
		case PT_LOAD:
			status = add_mapping(tracee, load_info, &program_header);
			if (status < 0)
				goto end;
			break;

		case PT_INTERP:
			status = add_interp(tracee, fd, load_info, &program_header);
			if (status < 0)
				goto end;
			break;

		default:
			break;
		}
	}

	status = 0;
end:
	if (fd >= 0)
		close(fd);

	return status;
}

/**
 * Add @load_base to each adresses of @load_info.
 */
static void add_load_base(LoadInfo *load_info, word_t load_base)
{
	size_t nb_mappings;
	size_t i;

	nb_mappings = talloc_array_length(load_info->mappings);
	for (i = 0; i < nb_mappings; i++)
		load_info->mappings[i].addr += load_base;

	if (IS_CLASS64(load_info->elf_header))
		load_info->elf_header.class64.e_entry += load_base;
	else
		load_info->elf_header.class32.e_entry += load_base;
}

/**
 * Compute the final load address for each position independant
 * objects of @tracee.
 *
 * TODO: support for ASLR.
 */
static void compute_load_addresses(Tracee *tracee)
{
	if (IS_POSITION_INDENPENDANT(tracee->load_info->elf_header))
		add_load_base(tracee->load_info, 0x400000);

	/* Nothing more to do?  */
	if (tracee->load_info->interp == NULL)
		return;

	if (IS_POSITION_INDENPENDANT(tracee->load_info->interp->elf_header))
		add_load_base(tracee->load_info->interp, 0x7ff000000000);
}

/**
 * Extract all the information that will be required by
 * translate_load_*().  This function returns -errno if an error
 * occured, otherwise 0.
 */
int translate_execve_enter(Tracee *tracee)
{
	char user_path[PATH_MAX];
	char host_path[PATH_MAX];
	int status;

	status = get_sysarg_path(tracee, user_path, SYSARG_1);
	if (status < 0)
		return status;

	status = expand_shebang(tracee, host_path, user_path);
	if (status < 0)
		/* The Linux kernel actually returns -EACCES when
		 * trying to execute a directory.  */
		return status == -EISDIR ? -EACCES : status;

	/* WIP.  */
	status = set_sysarg_path(tracee, "/usr/local/cedric/git/proot/src/execve/stub-x86_64", SYSARG_1);
	if (status < 0)
		return status;

	tracee->load_info = talloc_zero(tracee, LoadInfo);
	if (tracee->load_info == NULL)
		return -ENOMEM;

	tracee->load_info->path = talloc_strdup(tracee->load_info, host_path);
	if (tracee->load_info->path == NULL)
		return -ENOMEM;

	status = extract_load_info(tracee, tracee->load_info);
	if (status < 0)
		return status;

	if (tracee->load_info->interp != NULL) {
		status = extract_load_info(tracee, tracee->load_info->interp);
		if (status < 0)
			return status;

		/* An ELF interpreter is supposed to be
		 * standalone.  */
		if (tracee->load_info->interp->interp != NULL)
			return -EINVAL;
	}

	compute_load_addresses(tracee);

	/* Remember the value for "/proc/self/exe".  It points to a
	 * canonicalized guest path, hence detranslate_path() instead
	 * of using user_path directly.  */
	status = detranslate_path(tracee, host_path, NULL);
	if (status >= 0) {
		tracee->exe = talloc_strdup(tracee, host_path);
		if (tracee->exe != NULL)
			talloc_set_name_const(tracee->exe, "$exe");
	}

	return 0;
}

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
