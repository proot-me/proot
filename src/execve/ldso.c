/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#include <stdbool.h> /* bool, true, false, */
#include <string.h>  /* strlen(3), strcpy(3), strcat(3), strcmp(3), */
#include <stdlib.h>  /* getenv(3), */
#include <assert.h>  /* assert(3), */
#include <errno.h>   /* ENOMEM, */
#include <unistd.h>  /* close(2), */
#include <linux/limits.h>  /* PATH_MAX, ARG_MAX, */

#include "execve/ldso.h"
#include "execve/elf.h"
#include "execve/aoxp.h"
#include "tracee/tracee.h"
#include "cli/note.h"

/**
 * Check if the environment @variable has the given @name.
 */
bool is_env_name(const char *variable, const char *name)
{
	size_t length = strlen(name);

	return (variable[0] == name[0]
		&& length < strlen(variable)
		&& variable[length] == '='
		&& strncmp(variable, name, length) == 0);
}

/**
 * This function returns 1 or 0 depending on the equivalence of the
 * @reference environment variable and the one pointed to by the entry
 * in @envp at the given @index, otherwise it returns -errno when an
 * error occured.
 */
int compare_xpointee_env(ArrayOfXPointers *envp, size_t index, const char *reference)
{
	char *value;
	int status;

	assert(index < envp->length);

	status = read_xpointee_as_string(envp, index, &value);
	if (status < 0)
		return status;

	if (value == NULL)
		return 0;

	return (int)is_env_name(value, reference);
}

/**
 * This function ensures that environment variables related to the
 * dynamic linker are applied to the emulated program, not to QEMU
 * itself.  For instance, let's say the user has entered the
 * command-line below:
 *
 *     env LD_TRACE_LOADED_OBJECTS=1 /bin/ls
 *
 * It should be converted to:
 *
 *     qemu -E LD_TRACE_LOADED_OBJECTS=1 /bin/ls
 *
 * instead of:
 *
 *     env LD_TRACE_LOADED_OBJECTS=1 qemu /bin/ls
 *
 * Note that the LD_LIBRARY_PATH variable is always required to run
 * QEMU (a host binary):
 *
 *     env LD_LIBRARY_PATH=... qemu -U LD_LIBRARY_PATH /bin/ls
 *
 * or when LD_LIBRARY_PATH was also specified by the user:
 *
 *     env LD_LIBRARY_PATH=... qemu -E LD_LIBRARY_PATH=... /bin/ls
 *
 * This funtion returns -errno if an error occured, otherwise 0.
 */
int ldso_env_passthru(const Tracee *tracee, ArrayOfXPointers *envp, ArrayOfXPointers *argv,
		const char *define, const char *undefine, size_t offset)
{
	bool has_seen_library_path = false;
	int status;
	size_t i;

	for (i = 0; i < envp->length; i++) {
		bool is_known = false;
		char *env;

		status = read_xpointee_as_string(envp, i, &env);
		if (status < 0)
			return status;

		/* Skip variables that do not start with "LD_".  */
		if (env == NULL || strncmp(env, "LD_", sizeof("LD_") - 1) != 0)
			continue;

		/* When a host program executes a guest program, use
		 * the value of LD_LIBRARY_PATH as it was before being
		 * swapped by the mixed-mode support.  */
		if (   tracee->host_ldso_paths != NULL
		    && tracee->guest_ldso_paths != NULL
		    && is_env_name(env, "LD_LIBRARY_PATH")
		    && strcmp(env, tracee->host_ldso_paths) == 0)
			env = (char *) tracee->guest_ldso_paths;

#define PASSTHRU(check, name)						\
		if (is_env_name(env, name)) {				\
			check |= true;					\
			/* Errors are not fatal here.  */		\
			status = resize_array_of_xpointers(argv, offset, 2);	\
			if (status >= 0) {				\
				status = write_xpointees(argv, offset, 2, define, env); \
				if (status < 0)				\
					return status;			\
			}						\
			write_xpointee(envp, i, "");			\
			continue;					\
		}							\

		PASSTHRU(has_seen_library_path, "LD_LIBRARY_PATH");
		PASSTHRU(is_known, "LD_PRELOAD");
		PASSTHRU(is_known, "LD_BIND_NOW");
		PASSTHRU(is_known, "LD_TRACE_LOADED_OBJECTS");
		PASSTHRU(is_known, "LD_AOUT_LIBRARY_PATH");
		PASSTHRU(is_known, "LD_AOUT_PRELOAD");
		PASSTHRU(is_known, "LD_AUDIT");
		PASSTHRU(is_known, "LD_BIND_NOT");
		PASSTHRU(is_known, "LD_DEBUG");
		PASSTHRU(is_known, "LD_DEBUG_OUTPUT");
		PASSTHRU(is_known, "LD_DYNAMIC_WEAK");
		PASSTHRU(is_known, "LD_HWCAP_MASK");
		PASSTHRU(is_known, "LD_KEEPDIR");
		PASSTHRU(is_known, "LD_NOWARN");
		PASSTHRU(is_known, "LD_ORIGIN_PATH");
		PASSTHRU(is_known, "LD_POINTER_GUARD");
		PASSTHRU(is_known, "LD_PROFILE");
		PASSTHRU(is_known, "LD_PROFILE_OUTPUT");
		PASSTHRU(is_known, "LD_SHOW_AUXV");
		PASSTHRU(is_known, "LD_USE_LOAD_BIAS");
		PASSTHRU(is_known, "LD_VERBOSE");
		PASSTHRU(is_known, "LD_WARN");
	}

	if (!has_seen_library_path) {
		/* Errors are not fatal here.  */
		status = resize_array_of_xpointers(argv, offset, 2);
		if (status >= 0) {
			status = write_xpointees(argv, offset, 2, undefine, "LD_LIBRARY_PATH");
			if (status < 0)
				return status;
		}
	}

	return 0;
}

/**
 * Add to @host_ldso_paths the list of @paths prefixed with the path
 * to the host rootfs.
 */
static int add_host_ldso_paths(char host_ldso_paths[ARG_MAX], const char *paths)
{
	char *cursor1;
	const char *cursor2;

	cursor1 = host_ldso_paths + strlen(host_ldso_paths);
	cursor2 = paths;

	do {
		bool is_absolute;
		size_t length1;
		size_t length2 = strcspn(cursor2, ":");

		is_absolute = (*cursor2 == '/');

		length1 = 1 + length2;
		if (is_absolute)
			length1 += strlen(HOST_ROOTFS);

		/* Check there's enough room.  */
		if (cursor1 + length1 >= host_ldso_paths + ARG_MAX)
			return -ENOEXEC;

		if (cursor1 != host_ldso_paths) {
			strcpy(cursor1, ":");
			cursor1++;
		}

		/* Since we are executing a host binary under a
		 * QEMUlated environment, we have to access its
		 * library paths through the "host-rootfs" binding.
		 * Technically it means a path like "/lib" is accessed
		 * as "${HOST_ROOTFS}/lib" to avoid conflict with the
		 * guest "/lib".  */
		if (is_absolute) {
			strcpy(cursor1, HOST_ROOTFS);
			cursor1 += strlen(HOST_ROOTFS);
		}

		strncpy(cursor1, cursor2, length2);
		cursor1 += length2;

		cursor2 += length2 + 1;
	} while (*(cursor2 - 1) != '\0');

	*cursor1 = '\0';

	return 0;
}

struct find_program_header_data {
	ProgramHeader *program_header;
	SegmentType type;
	uint64_t address;
};

/**
 * This function is a program header iterator.  It stops the iteration
 * (by returning 1) once it has found a program header that matches
 * @data.  This function returns -errno if an error occurred,
 * otherwise 0 or 1.
 */
static int find_program_header(const ElfHeader *elf_header,
			const ProgramHeader *program_header, void *data_)
{
	struct find_program_header_data *data = data_;

	if (PROGRAM_FIELD(*elf_header, *program_header, type) == data->type) {
		uint64_t start;
		uint64_t end;

		memcpy(data->program_header, program_header, sizeof(ProgramHeader));

		if (data->address == (uint64_t) -1)
			return 1;

		start = PROGRAM_FIELD(*elf_header, *program_header, vaddr);
		end   = start + PROGRAM_FIELD(*elf_header, *program_header, memsz);

		if (start < end
			&& data->address >= start
			&& data->address <= end)
			return 1;
	}

	return 0;
}

/**
 * Add to @xpaths the paths (':'-separated list) from the file
 * referenced by @fd at the given @offset.  This function returns
 * -errno if an error occured, otherwise 0.
 */
static int add_xpaths(const Tracee *tracee, int fd, uint64_t offset, char **xpaths)
{
	char *paths = NULL;
	char *tmp;

	size_t length;
	size_t size;
	int status;

	status = (int) lseek(fd, offset, SEEK_SET);
	if (status < 0)
		return -errno;

	/* Read the complete list of paths.  */
	length = 0;
	paths = NULL;
	do {
		size = length + 1024;

		tmp = talloc_realloc(tracee->ctx, paths, char, size);
		if (!tmp)
			return -ENOMEM;
		paths = tmp;

		status = read(fd, paths + length, 1024);
		if (status < 0)
			return status;

		length += strnlen(paths + length, 1024);
	} while (length == size);

	/* Concatene this list of paths to xpaths.  */
	if (!*xpaths) {
		*xpaths = talloc_array(tracee->ctx, char, length + 1);
		if (!*xpaths)
			return -ENOMEM;

		strcpy(*xpaths, paths);
	}
	else {
		length += strlen(*xpaths);
		length++; /* ":" separator */

		tmp = talloc_realloc(tracee->ctx, *xpaths, char, length + 1);
		if (!tmp)
			return -ENOMEM;
		*xpaths = tmp;

		strcat(*xpaths, ":");
		strcat(*xpaths, paths);
	}

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
static int read_ldso_rpaths(const Tracee* tracee, int fd, const ElfHeader *elf_header,
		char **rpaths, char **runpaths)
{
	ProgramHeader dynamic_segment;
	ProgramHeader strtab_segment;
	struct find_program_header_data data;
	uint64_t strtab_address = (uint64_t) -1;
	off_t strtab_offset;
	int status;
	size_t i;

	uint64_t offsetof_dynamic_segment;
	uint64_t sizeof_dynamic_segment;
	size_t sizeof_dynamic_entry;

	data.program_header = &dynamic_segment;
	data.type = PT_DYNAMIC;
	data.address = (uint64_t) -1;

	status = iterate_program_headers(tracee, fd, elf_header, find_program_header, &data);
	if (status <= 0)
		return status;

	offsetof_dynamic_segment = PROGRAM_FIELD(*elf_header, dynamic_segment, offset);
	sizeof_dynamic_segment   = PROGRAM_FIELD(*elf_header, dynamic_segment, filesz);

	if (IS_CLASS32(*elf_header))
		sizeof_dynamic_entry = sizeof(DynamicEntry32);
	else
		sizeof_dynamic_entry = sizeof(DynamicEntry64);

	if (sizeof_dynamic_segment % sizeof_dynamic_entry != 0)
		return -ENOEXEC;

/**
 * Invoke @embedded_code on each dynamic entry of the given @type.
 */
#define FOREACH_DYNAMIC_ENTRY(type, embedded_code)					\
	for (i = 0; i < sizeof_dynamic_segment / sizeof_dynamic_entry; i++) {		\
		DynamicEntry dynamic_entry;						\
		uint64_t value;								\
											\
		/* embedded_code may change the file offset.  */			\
		status = (int) lseek(fd, offsetof_dynamic_segment + i * sizeof_dynamic_entry, \
				SEEK_SET);						\
		if (status < 0)								\
			return -errno;							\
											\
		status = read(fd, &dynamic_entry, sizeof_dynamic_entry);		\
		if (status < 0)								\
			return status;							\
											\
		if (DYNAMIC_FIELD(*elf_header, dynamic_entry, tag) != type)		\
			continue;							\
											\
		value =	DYNAMIC_FIELD(*elf_header, dynamic_entry, val);			\
											\
		embedded_code								\
	}

	/* Get the address of the *first* string table.  The ELF
	 * specification doesn't mention if it may have several string
	 * table references.  */
	FOREACH_DYNAMIC_ENTRY(DT_STRTAB, {
		strtab_address = value;
		break;
	})

	if (strtab_address == (uint64_t) -1)
		return 0;

	data.program_header = &strtab_segment;
	data.type = PT_LOAD;
	data.address = strtab_address;

	/* Search the program header that contains the given string table.  */
	status = iterate_program_headers(tracee, fd, elf_header, find_program_header, &data);
	if (status < 0)
		return status;

	strtab_offset = PROGRAM_FIELD(*elf_header, strtab_segment, offset)
		+ (strtab_address - PROGRAM_FIELD(*elf_header, strtab_segment, vaddr));

	FOREACH_DYNAMIC_ENTRY(DT_RPATH,	{
		if (strtab_offset < 0 || (uint64_t) strtab_offset > UINT64_MAX - value)
			return -ENOEXEC;

		status = add_xpaths(tracee, fd, strtab_offset + value, rpaths);
		if (status < 0)
			return status;
	})

	FOREACH_DYNAMIC_ENTRY(DT_RUNPATH, {
		if (strtab_offset < 0 || (uint64_t) strtab_offset > UINT64_MAX - value)
			return -ENOEXEC;

		status = add_xpaths(tracee, fd, strtab_offset + value, runpaths);
		if (status < 0)
			return status;
	})

#undef FOREACH_DYNAMIC_ENTRY

	return 0;
}

/**
 * Rebuild the variable LD_LIBRARY_PATH in @envp for the program
 * @host_path according to its RPATH, RUNPATH, and the initial
 * LD_LIBRARY_PATH.  This function returns -errno if an error occured,
 * 1 if RPATH/RUNPATH entries were found, 0 otherwise.
 */
int rebuild_host_ldso_paths(Tracee *tracee, const char host_path[PATH_MAX], ArrayOfXPointers *envp)
{
	static char *initial_ldso_paths = NULL;
	ElfHeader elf_header;

	char host_ldso_paths[ARG_MAX] = "";
	bool rpath_found = false;

	char *rpaths   = NULL;
	char *runpaths = NULL;

	size_t length1;
	size_t length2;

	size_t index;
	int status;
	int fd;

	fd = open_elf(host_path, &elf_header);
	if (fd < 0)
		return fd;

	status = read_ldso_rpaths(tracee, fd, &elf_header, &rpaths, &runpaths);
	close(fd);
	if (status < 0)
		return status;

	/* 1. DT_RPATH  */
	if (rpaths && !runpaths) {
		status = add_host_ldso_paths(host_ldso_paths, rpaths);
		if (status < 0)
			return 0; /* Not fatal.  */
		rpath_found = true;
	}

	/* 2. LD_LIBRARY_PATH  */
	if (initial_ldso_paths == NULL)
		initial_ldso_paths = strdup(getenv("LD_LIBRARY_PATH") ?: "/");
	if (initial_ldso_paths != NULL && initial_ldso_paths[0] != '\0') {
		status = add_host_ldso_paths(host_ldso_paths, initial_ldso_paths);
		if (status < 0)
			return 0; /* Not fatal.  */
	}

	/* 3. DT_RUNPATH  */
	if (runpaths) {
		status = add_host_ldso_paths(host_ldso_paths, runpaths);
		if (status < 0)
			return 0; /* Not fatal.  */
		rpath_found = true;
	}

	/* 4. /etc/ld.so.cache NYI.  */

	/* 5. /lib[32|64], /usr/lib[32|64] + /usr/local/lib[32|64]  */
	/* 6. /lib, /usr/lib + /usr/local/lib  */
	if (IS_CLASS32(elf_header))
		status = add_host_ldso_paths(host_ldso_paths,
#if defined(ARCH_X86) || defined(ARCH_X86_64)
					"/lib/i386-linux-gnu:/usr/lib/i386-linux-gnu:"
#endif
					"/lib32:/usr/lib32:/usr/local/lib32"
					":/lib:/usr/lib:/usr/local/lib");
	else
		status = add_host_ldso_paths(host_ldso_paths,
#if defined(ARCH_X86_64)
					"/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:"
#elif defined(ARCH_ARM64)
					"/lib/aarch64-linux-gnu:/usr/lib/aarch64-linux-gnu:"
#endif
					"/lib64:/usr/lib64:/usr/local/lib64"
					":/lib:/usr/lib:/usr/local/lib");
	if (status < 0)
		return 0; /* Not fatal.  */

	status = find_xpointee(envp, "LD_LIBRARY_PATH");
	if (status < 0)
		return 0; /* Not fatal.  */
	index = (size_t) status;

	if (index == envp->length) {
		/* Allocate a new entry at the end of envp[] when
		 * LD_LIBRARY_PATH was not found.  */

		index = (envp->length > 0 ? envp->length - 1 : 0);
		status = resize_array_of_xpointers(envp, index, 1);
		if (status < 0)
			return 0; /* Not fatal.  */
	}
	else if (tracee->guest_ldso_paths == NULL) {
		/* Remember guest LD_LIBRARY_PATH in order to restore
		 * it when a host program will execute a guest
		 * program.  */
		char *env;

		/* Errors are not fatal here.  */
		status = read_xpointee_as_string(envp, index, &env);
		if (status >= 0)
			tracee->guest_ldso_paths = talloc_strdup(tracee, env);
	}

	/* Forge the new LD_LIBRARY_PATH variable from
	 * host_ldso_paths.  */
	length1 = strlen("LD_LIBRARY_PATH=");
	length2 = strlen(host_ldso_paths);
	if (ARG_MAX - length2 - 1 < length1)
		return 0; /* Not fatal.  */

	memmove(host_ldso_paths + length1, host_ldso_paths, length2 + 1);
	memcpy(host_ldso_paths, "LD_LIBRARY_PATH=" , length1);

	write_xpointee(envp, index, host_ldso_paths);

	/* The guest LD_LIBRARY_PATH will be restored only if the host
	 * program didn't change it explicitly, so remember its
	 * initial value.  */
	if (tracee->host_ldso_paths == NULL)
		tracee->host_ldso_paths = talloc_strdup(tracee, host_ldso_paths);

	return (int) rpath_found;
}
