/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
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

#include <fcntl.h>  /* open(2), */
#include <unistd.h> /* read(2), close(2), */
#include <errno.h>  /* EACCES, ENOTSUP, */
#include <stdint.h> /* UINT64_MAX, */
#include <limits.h> /* PATH_MAX, */
#include <string.h> /* strnlen(3), strcat(3), strcpy(3), */
#include <assert.h> /* assert(3), */
#include <stdlib.h> /* realloc(3), */
#include <string.h> /* strnlen(3), */

#include "execve/elf.h"
#include "notice.h"
#include "arch.h"
#include "config.h"

#include "compat.h"

/**
 * Open the ELF file @t_path and extract its header into @elf_header.
 * This function returns -errno if an error occured, otherwise the
 * file descriptor for @t_path.
 */
int open_elf(const char *t_path, union elf_header *elf_header)
{
	int fd;
	int status;

	/*
	 * Read the ELF header.
	 */

	fd = open(t_path, O_RDONLY);
	if (fd < 0)
		return -errno;

	status = read(fd, elf_header, sizeof(union elf_header));
	if (status != sizeof(union elf_header)) {
		status = -EACCES;
		goto end;
	}

	/* Check if it is an ELF file.  */
	if (   ELF_IDENT(*elf_header, 0) != 0x7f
	    || ELF_IDENT(*elf_header, 1) != 'E'
	    || ELF_IDENT(*elf_header, 2) != 'L'
	    || ELF_IDENT(*elf_header, 3) != 'F') {
		status = -ENOEXEC;
		goto end;
	}

	/* Check if it is a known class (32-bit or 64-bit).  */
	if (   !IS_CLASS32(*elf_header)
	    && !IS_CLASS64(*elf_header)) {
		status = -ENOEXEC;
		goto end;
	}

	status = 0;
end:
	/* Delayed error handling.  */
	if (status < 0) {
		close(fd);
		return status;
	}

	return fd;
}

/**
 * Find in the file referenced by @fd -- which has the provided
 * @elf_header -- the first @program_header of a given @type and
 * loaded at the given @address (-1 for wherever).  This function
 * returns -errno if an error occured, 1 if the program header was
 * found, otherwise 0.
 */
int find_program_header(int fd,
			const union elf_header *elf_header,
			union program_header *program_header,
			enum segment_type type, uint64_t address)
{
	uint64_t elf_phoff;
	uint16_t elf_phentsize;
	uint16_t elf_phnum;

	int status;
	int i;

	/* Get class-specific fields. */
	elf_phnum     = ELF_FIELD(*elf_header, phnum);
	elf_phentsize = ELF_FIELD(*elf_header, phentsize);
	elf_phoff     = ELF_FIELD(*elf_header, phoff);

	/*
	 * Some sanity checks regarding the current
	 * support of the ELF specification in PRoot.
	 */

	if (elf_phnum >= 0xffff) {
		notice(WARNING, INTERNAL, "%d: big PH tables are not yet supported.", fd);
		return -ENOTSUP;
	}

	if (!KNOWN_PHENTSIZE(*elf_header, elf_phentsize)) {
		notice(WARNING, INTERNAL, "%d: unsupported size of program header.", fd);
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
		status = read(fd, program_header, elf_phentsize);
		if (status != elf_phentsize)
			status = -errno;

		if (PROGRAM_FIELD(*elf_header, *program_header, type) == type) {
			uint64_t start;
			uint64_t end;

			if (address == (uint64_t) -1)
				return 1;

			start = PROGRAM_FIELD(*elf_header, *program_header, vaddr);
			end   = start + PROGRAM_FIELD(*elf_header, *program_header, memsz);

			if (start < end
			    && address >= start
			    && address <= end)
				return 1;
		}
	}

	return 0;
}

/**
 * Check if @t_path is an ELF file for the host architecture.
 */
bool is_host_elf(const char *t_path)
{
	int host_elf_machine[] = HOST_ELF_MACHINE;
	union elf_header elf_header;
	uint16_t elf_machine;
	int fd;
	int i;

	if (!config.host_rootfs)
		return false;

	fd = open_elf(t_path, &elf_header);
	if (fd < 0)
		return false;
	close(fd);

	elf_machine = ELF_FIELD(elf_header, machine);
	for (i = 0; host_elf_machine[i] != 0; i++) {
		if (host_elf_machine[i] == elf_machine) {
			VERBOSE(1, "'%s' is a host ELF", t_path);
			return true;
		}
	}

	return false;
}

/**
 * Invoke @callback on each dynamic entry of the given @type for the
 * file referenced by @fd -- which has the provided @elf_header and
 * @program_header (type == PT_DYNAMIC) --.  The iteration is stopped
 * if @callback returns an error (something < 0).  This function
 * returns -errno if an error occured, otherwise 0.
 */
static int foreach_dynamic_entry(int fd,
			const union elf_header *elf_header,
			const union program_header *program_header,
			enum dynamic_type type, int (*callback)(uint64_t))
{
	union dynamic_entry dynamic_entry;
	size_t sizeof_dynamic_entry;
	uint64_t offset;
	uint64_t size;
	int i;

	assert(elf_header);
	assert(program_header);
	assert(callback);

	assert(PROGRAM_FIELD(*elf_header, *program_header, type) == PT_DYNAMIC);

	offset = PROGRAM_FIELD(*elf_header, *program_header, offset);
	size   = PROGRAM_FIELD(*elf_header, *program_header, filesz);

	if (IS_CLASS32(*elf_header))
		sizeof_dynamic_entry = sizeof(struct dynamic_entry32);
	else
		sizeof_dynamic_entry = sizeof(struct dynamic_entry64);

	if (size % sizeof_dynamic_entry != 0)
		return -ENOEXEC;

	for (i = 0; i < size / sizeof_dynamic_entry; i++) {
		int status;

		/* callback() may change the file offset.  */
		status = (int) lseek(fd, offset + i * sizeof_dynamic_entry, SEEK_SET);
		if (status < 0)
			return -errno;

		status = read(fd, &dynamic_entry, sizeof_dynamic_entry);
		if (status < 0)
			return status;

		if (DYNAMIC_FIELD(*elf_header, dynamic_entry, tag) != type)
			continue;

		status = callback(DYNAMIC_FIELD(*elf_header, dynamic_entry, val));
		if (status < 0)
			return status;
	}

	return 0;
}

/**
 * Add to @xpaths the paths (':'-separated list) from the file
 * referenced by @fd at the given @offset.  This function returns
 * -errno if an error occured, otherwise 0.
 */
static int add_xpaths(int fd, uint64_t offset, char **xpaths)
{
	char *paths = NULL;
	char *tmp;

	size_t length;
	size_t size;
	int status;

	status = (int) lseek(fd, offset, SEEK_SET);
	if (status < 0) {
		status = -errno;
		goto end;
	}

	/* Read the complete list of paths.  */
	length = 0;
	paths = NULL;
	do {
		size = length + 1024;

		tmp = realloc(paths, size);
		if (!tmp) {
			status = -ENOMEM;
			goto end;
		}
		paths = tmp;

		status = read(fd, paths + length, 1024);
		if (status < 0)
			goto end;

		length += strnlen(paths + length, 1024);
	} while (length == size);

	/* Concatene this list of paths to xpaths.  */
	if (!*xpaths) {
		*xpaths = malloc(length + 1);
		if (!*xpaths) {
			status = -ENOMEM;
			goto end;
		}

		strcpy(*xpaths, paths);
	}
	else {
		length += strlen(*xpaths);
		length++; /* ":" separator */

		tmp = realloc(*xpaths, length + 1);
		if (!tmp) {
			status = -ENOMEM;
			goto end;
		}
		*xpaths = tmp;

		strcat(*xpaths, ":");
		strcat(*xpaths, paths);
	}

end:
	if (paths)
		free(paths);

	if (status < 0)
		return status;

	/* I don't know if DT_R*PATH entries are unique.  In
	 * doubt I support multiple entries.  */
	return 0;
}

/**
 * Put the RPATH and RUNPATH dynamic entries from the file referenced
 * by @fd -- which has the provided @elf_header -- in @rpaths and
 * @runpaths respectively.  This function returns -errno if an error
 * occured, otherwise 0.
 */
int read_ldso_rpaths(int fd, const union elf_header *elf_header,
		char **rpaths, char **runpaths)
{
	union program_header dynamic;
	union program_header strtab_segment;
	uint64_t strtab_address = (uint64_t) -1;
	off_t strtab_offset;
	int status;

	status = find_program_header(fd, elf_header, &dynamic, PT_DYNAMIC, (uint64_t) -1);
	if (status <= 0)
		return status;

	/* Callback used to get the address of the *first* string
	 * table.  The ELF specification doesn't mention if it may
	 * have several string table references.  */
	int get_strtab_address(uint64_t value)
	{
		strtab_address = value;
		return -1; /* Stop the loop.  */
	}

	status = foreach_dynamic_entry(fd, elf_header, &dynamic, DT_STRTAB, get_strtab_address);
	if (strtab_address == (uint64_t) -1) {
		if (status < 0)
			return status;
		else
			return 0;
	}

	/* Search the program header that contains the given string table.  */
	status = find_program_header(fd, elf_header, &strtab_segment, PT_LOAD, strtab_address);
	if (status < 0)
		return status;

	strtab_offset = PROGRAM_FIELD(*elf_header, strtab_segment, offset)
		+ (strtab_address - PROGRAM_FIELD(*elf_header, strtab_segment, vaddr));

	int add_rpaths(uint64_t index)
	{
		if (strtab_offset > UINT64_MAX - index)
			return -ENOEXEC;
		return add_xpaths(fd, strtab_offset + index, rpaths);
	}

	int add_runpaths(uint64_t index)
	{
		if (strtab_offset > UINT64_MAX - index)
			return -ENOEXEC;
		return add_xpaths(fd, strtab_offset + index, runpaths);
	}

	status = foreach_dynamic_entry(fd, elf_header, &dynamic, DT_RPATH, add_rpaths);
	if (status < 0)
		return status;

	status = foreach_dynamic_entry(fd, elf_header, &dynamic, DT_RUNPATH, add_runpaths);
	if (status < 0)
		return status;

	return 0;
}
