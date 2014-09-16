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
#include <sys/mman.h>   /* PROT_*, */
#include <assert.h>     /* assert(3), */

#include "execve/execve.h"
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
 * Extract the load map of @object->path.  This function returns
 * -errno if an error occured, otherwise it returns 0.  On success,
 * both @object->mappings and @object->interp are filled according to
 * the content of @object->path.
 *
 * TODO: factorize with find_program_header()
 */
static int extract_load_map(const Tracee *tracee, LoadMap *object)
{
	ElfHeader elf_header;
	ProgramHeader program_header;

	uint64_t elf_phoff;
	uint16_t elf_phentsize;
	uint16_t elf_phnum;

	int status;
	int fd;
	int i;

	assert(object != NULL);
	assert(object->path != NULL);

	fd = open_elf(object->path, &elf_header);
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

			if (object->mappings == NULL) {
				object->mappings = talloc_array(object, Mapping, 1);
				index = 0;
			}
			else {
				index = talloc_array_length(object->mappings);
				object->mappings = talloc_realloc(object, object->mappings,
								Mapping, index + 1);
			}
			if (object->mappings == NULL)
				return -ENOMEM;

			object->mappings[index].file.base = P(offset);
			object->mappings[index].file.size = P(filesz);
			object->mappings[index].mem.base  = P(vaddr);
			object->mappings[index].mem.size  = P(memsz);
			object->mappings[index].flags = (0
							| (P(flags) & PF_R ? PROT_READ  : 0)
							| (P(flags) & PF_W ? PROT_WRITE : 0)
							| (P(flags) & PF_X ? PROT_EXEC  : 0));
			break;
		}

		case PT_INTERP: {
			/* Only one PT_INTERP segment is allowed.  */
			if (object->interp != NULL)
				return -EINVAL;

			object->interp = talloc_zero(object, LoadMap);
			if (object->interp == NULL)
				return -ENOMEM;

			object->interp->path = talloc_size(object, P(filesz) + 1);
			if (object->interp->path == NULL)
				return -ENOMEM;

			/* Remember pread(2) doesn't change the
			 * current position in the file.  */
			status = pread(fd, object->interp->path, P(filesz), P(offset));
			if ((size_t) status != P(filesz)) /* Unexpected size.  */
				status = -EACCES;
			if (status < 0)
				return status;

			object->interp->path[P(filesz)] = '\0';

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
 * Print to stderr the load map of @object.
 */
static void print_load_map(const LoadMap *object)
{
	size_t length;
	size_t i;

	length = talloc_array_length(object);

	for (i = 0; i <= length; i++) {
		fprintf(stderr, "%016lx-%016lx %c%c%cp %016lx 00:00 0 %s\n",
			object->mappings[i].mem.base,
			object->mappings[i].mem.base + object->mappings[i].mem.size,
			object->mappings[i].flags & PROT_READ  ? 'r' : '-',
			object->mappings[i].flags & PROT_WRITE ? 'w' : '-',
			object->mappings[i].flags & PROT_EXEC  ? 'x' : '-',
			object->mappings[i].file.base, object->path);
	}

	if (object->interp != NULL)
		print_load_map(object->interp);
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
	LoadMap *object;
	int status;

	status = get_sysarg_path(tracee, user_path, SYSARG_1);
	if (status < 0)
		return status;

	status = extract_shebang(tracee, host_path, user_path, NULL);
	if (status < 0)
		return status;

	/*tracee->*/object = talloc_zero(tracee, LoadMap);
	if (object == NULL)
		return -ENOMEM;

	object->path = talloc_strdup(object, host_path);

	status = extract_load_map(tracee, object);
	if (status < 0)
		return status;

	if (object->interp != NULL) {
		status = extract_load_map(tracee, object->interp);
		if (status < 0)
			return status;

		/* An ELF interpreter is supposed to be
		 * standalone.  */
		if (object->interp->interp != NULL)
			return -EINVAL;
	}

	print_load_map(object);

	return 0;
}
