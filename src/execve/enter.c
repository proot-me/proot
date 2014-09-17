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

#include "execve/execve.h"
#include "execve/load.h"
#include "execve/elf.h"
#include "path/path.h"
#include "tracee/tracee.h"
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

#define P(a) PROGRAM_FIELD(elf_header, program_header, a)

	for (i = 0; i < elf_phnum; i++) {
		status = read(fd, &program_header, elf_phentsize);
		if (status != elf_phentsize)
			return (status < 0 ? -errno : -ENOTSUP);

		switch (P(type)) {
		case PT_LOAD: {
			size_t index;

			if (load->mappings == NULL) {
				load->mappings = talloc_array(load, Mapping, 1);
				index = 0;
			}
			else {
				index = talloc_array_length(load->mappings);
				load->mappings = talloc_realloc(load, load->mappings,
								Mapping, index + 1);
			}
			if (load->mappings == NULL)
				return -ENOMEM;

			load->mappings[index].file.base = P(offset);
			load->mappings[index].file.size = P(filesz);
			load->mappings[index].mem.base  = P(vaddr);
			load->mappings[index].mem.size  = P(memsz);
			load->mappings[index].flags     = P(flags);
			break;
		}

		case PT_INTERP: {
			char *user_path;
			char host_path[PATH_MAX];

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

			break;
		}

		default:
			break;
		}
	}

#undef P

	return 0;
}

/**
 * Extract all the information that will be required by
 * translate_execve_exit().  This function returns -errno if an error
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

	tracee->load->path = talloc_strdup(tracee->load, host_path);

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
