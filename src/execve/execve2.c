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

#include <sys/types.h>  /* lstat(2), lseek(2), open(2), */
#include <sys/stat.h>   /* lstat(2), lseek(2), open(2), */
#include <unistd.h>     /* access(2), lstat(2), close(2), read(2), */
#include <errno.h>      /* E*, */
#include <fcntl.h>      /* open(2), */
#include <assert.h>     /* assert(3), */
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* PROT_*, */

#include "execve/execve.h"
#include "execve/load.h"
#include "execve/elf.h"
#include "path/path.h"
#include "tracee/tracee.h"
#include "syscall/chain.h"
#include "syscall/syscall.h"
#include "cli/notice.h"

/**
 * Translate @user_path into @host_path and check if this latter
 * exists, is executable and is a regular file.  This function returns
 * -errno if an error occured, 0 otherwise.
 */
static int translate_n_check(Tracee *tracee, char host_path[PATH_MAX], const char *user_path)
{
	struct stat statl;
	int status;

	status = translate_path(tracee, host_path, AT_FDCWD, user_path, true);
	if (status < 0)
		return status;

	status = access(host_path, F_OK);
	if (status < 0)
		return -ENOENT;

	status = access(host_path, X_OK);
	if (status < 0)
		return -EACCES;

	status = lstat(host_path, &statl);
	if (status < 0)
		return -EPERM;

	return 0;
}

/**
 * Extract the shebang of @user_path.  This function returns -errno if
 * an error occured, otherwise it returns 0.  On success, @host_path
 * points to the initial program or to its script interpreter if any,
 * and @args contains the script interpreter arguments.
 *
 * TODO:
 *
 * - translate @user_path -> @host_path
 *
 * - extract the shebang from @host_path -> @user_path, if any
 *
 * - extract the shebang arguments -> @args, if any
 *
 * - translate the new @user_path -> @host_path
 *
 */
static int extract_shebang(Tracee *tracee, char host_path[PATH_MAX],
			const char user_path[PATH_MAX], char **args UNUSED)
{
	int status;

	status = translate_n_check(tracee, host_path, user_path);
	if (status < 0)
		return status;

	return 0;
}

#define P(a) PROGRAM_FIELD(*elf_header, *program_header, a)

/**
 * Add @program_header (type PT_LOAD) to @load->mappings.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_mapping(const Tracee *tracee UNUSED, LoadMap *load,
			const ElfHeader *elf_header, const ProgramHeader *program_header)
{
	size_t index;

	static word_t page_size = 0;
	static word_t page_mask = 0;

	if (page_size == 0) {
		page_size = sysconf(_SC_PAGE_SIZE);
		if ((int) page_size <= 0)
			page_size = 0x1000;
		page_mask = ~(page_size - 1);
	}


	if (load->mappings == NULL)
		index = 0;
	else
		index = talloc_array_length(load->mappings);

	load->mappings = talloc_realloc(load, load->mappings, Mapping, index + 1);
	if (load->mappings == NULL)
		return -ENOMEM;

	load->mappings[index].fd     = -1; /* Unknown yet.  */
	load->mappings[index].offset =  P(offset) & page_mask;
	load->mappings[index].addr   =  P(vaddr)  & page_mask;
	load->mappings[index].length = (P(filesz) + page_size) & page_mask;
	load->mappings[index].flags  = MAP_PRIVATE;
	load->mappings[index].prot   =  ((P(flags) & PF_R ? PROT_READ  : 0)
					| (P(flags) & PF_W ? PROT_WRITE : 0)
					| (P(flags) & PF_X ? PROT_EXEC  : 0));

	/* "If the segment's memory size p_memsz is larger than the
	 * file size p_filesz, the "extra" bytes are defined to hold
	 * the value 0 and to follow the segment's initialized area."
	 * -- man 7 elf.  */
	if (P(memsz) > P(filesz)) {
		index++;
		load->mappings = talloc_realloc(load, load->mappings, Mapping, index + 1);
		if (load->mappings == NULL)
			return -ENOMEM;

		load->mappings[index].fd     = -1;  /* Anonymous.  */
		load->mappings[index].offset =  0;
		load->mappings[index].addr   = (P(vaddr) + P(memsz) + page_size) & page_mask;
		load->mappings[index].length = (P(memsz) - P(filesz) + page_size) & page_mask;
		load->mappings[index].flags  = MAP_PRIVATE | MAP_ANONYMOUS;
		load->mappings[index].prot   = load->mappings[index - 1].prot;
	}

	return 0;
}

/**
 * Add @program_header (type PT_INTERP) to @load->interp.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_interp(Tracee *tracee, int fd, LoadMap *load,
		const ElfHeader *elf_header, const ProgramHeader *program_header)
{
	char host_path[PATH_MAX];
	char *user_path;
	int status;

	/* Only one PT_INTERP segment is allowed.  */
	if (load->interp != NULL)
		return -EINVAL;

	load->interp = talloc_zero(load, LoadMap);
	if (load->interp == NULL)
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

	status = translate_n_check(tracee, host_path, user_path);
	if (status < 0)
		return status;

	load->interp->path = talloc_strdup(load->interp, host_path);
	if (load->interp->path == NULL)
		return -ENOMEM;

	return 0;
}

#undef P

/**
 * Extract the load map of @load->path.  This function returns
 * -errno if an error occured, otherwise it returns 0.  On success,
 * both @load->mappings and @load->interp are filled according to
 * the content of @load->path.
 *
 * TODO: factorize with find_program_header()
 */
static int extract_load_map(Tracee *tracee, LoadMap *load)
{
	ElfHeader elf_header;
	ProgramHeader program_header;

	uint64_t elf_phoff;
	uint16_t elf_phentsize;
	uint16_t elf_phnum;

	int status;
	int fd;
	int i;

	assert(load != NULL);
	assert(load->path != NULL);

	fd = open_elf(load->path, &elf_header);
	if (fd < 0)
		return fd;

	load->position_independent = (ELF_FIELD(elf_header, type) == ET_DYN);

	/* Get class-specific fields. */
	elf_phnum     = ELF_FIELD(elf_header, phnum);
	elf_phentsize = ELF_FIELD(elf_header, phentsize);
	elf_phoff     = ELF_FIELD(elf_header, phoff);

	/*
	 * Some sanity checks regarding the current
	 * support of the ELF specification in PRoot.
	 */

	if (elf_phnum >= 0xffff) {
		notice(tracee, WARNING, INTERNAL, "%d: big PH tables are not yet supported.", fd);
		return -ENOTSUP;
	}

	if (!KNOWN_PHENTSIZE(elf_header, elf_phentsize)) {
		notice(tracee, WARNING, INTERNAL, "%d: unsupported size of program header.", fd);
		return -ENOTSUP;
	}

	/*
	 * Search the first entry of the requested type into the
	 * program header table.
	 */

	status = (int) lseek(fd, elf_phoff, SEEK_SET);
	if (status < 0)
		return -errno;

	for (i = 0; i < elf_phnum; i++) {
		status = read(fd, &program_header, elf_phentsize);
		if (status != elf_phentsize)
			return (status < 0 ? -errno : -ENOTSUP);

		switch (PROGRAM_FIELD(elf_header, program_header, type)) {
		case PT_LOAD:
			status = add_mapping(tracee, load, &elf_header, &program_header);
			if (status < 0)
				return status;
			break;

		case PT_INTERP:
			status = add_interp(tracee, fd, load, &elf_header, &program_header);
			if (status < 0)
				return status;
			break;

		default:
			break;
		}
	}

	return 0;
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

	status = extract_shebang(tracee, host_path, user_path, NULL);
	if (status < 0)
		return status;

	tracee->load = talloc_zero(tracee, LoadMap);
	if (tracee->load == NULL)
		return -ENOMEM;

	/* WIP.  */
	status = set_sysarg_path(tracee, "/usr/local/cedric/git/proot/src/execve/stub-x86_64", SYSARG_1);
	if (status < 0)
		return status;

	tracee->load->path = talloc_strdup(tracee->load, host_path);
	if (tracee->load->path == NULL)
		return -ENOMEM;

	status = extract_load_map(tracee, tracee->load);
	if (status < 0)
		return status;

	if (tracee->load->interp != NULL) {
		status = extract_load_map(tracee, tracee->load->interp);
		if (status < 0)
			return status;

		/* An ELF interpreter is supposed to be
		 * standalone.  */
		if (tracee->load->interp->interp != NULL)
			return -EINVAL;
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

	tracee->restore_original_regs = false;

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
	tracee->loading.map  = tracee->load;

	return 0;
}
