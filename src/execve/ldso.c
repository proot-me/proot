/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2013 STMicroelectronics
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
#include "execve/execve.h"
#include "execve/elf.h"
#include "tracee/tracee.h"
#include "tracee/array.h"
#include "notice.h"

/**
 * Check if the environment @variable has the given @name.
 */
static inline bool is_env_name(const char *variable, const char *name)
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
 * in @array at the given @index, otherwise it returns -errno when an
 * error occured.
 */
int compare_item_env(Array *array, size_t index, const char *reference)
{
	char *value;
	int status;

	assert(index < array->length);

	status = read_item_string(array, index, &value);
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
int ldso_env_passthru(Array *envp, Array *argv, const char *define, const char *undefine)
{
	bool has_seen_library_path = false;
	int status;
	size_t i;

	for (i = 0; i < envp->length; i++) {
		bool is_known = false;
		char *env;

		status = read_item_string(envp, i, &env);
		if (status < 0)
			return status;

		/* Skip variables that do not start with "LD_".  */
		if (env == NULL || strncmp(env, "LD_", sizeof("LD_") - 1) != 0)
			continue;

#define PASSTHRU(check, name)						\
		if (is_env_name(env, name)) {				\
			check |= true;					\
			/* Errors are not fatal here.  */		\
			status = resize_array(argv, 1, 2);		\
			if (status >= 0) {				\
				status = write_items(argv, 1, 2, define, env); \
				if (status < 0)				\
					return status;			\
			}						\
			write_item(envp, i, "");			\
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
		status = resize_array(argv, 1, 2);
		if (status >= 0) {
			status = write_items(argv, 1, 2, undefine, "LD_LIBRARY_PATH");
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

	*(cursor1++) = '\0';

	return 0;
}

/**
 * Rebuild the variable LD_LIBRARY_PATH in @envp for @t_program
 * according to its RPATH, RUNPATH, and the initial LD_LIBRARY_PATH.
 * This function returns -errno if an error occured, 1 if
 * RPATH/RUNPATH entries were found, 0 otherwise.
 */
int rebuild_host_ldso_paths(const Tracee *tracee, const char t_program[PATH_MAX], Array *envp)
{
	static char *initial_ldso_paths = NULL;
	ElfHeader elf_header;

	char host_ldso_paths[ARG_MAX] = "";
	bool inhibit_rpath = false;

	char *rpaths   = NULL;
	char *runpaths = NULL;

	size_t length1;
	size_t length2;

	size_t index;
	int status;
	int fd;

	fd = open_elf(t_program, &elf_header);
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
		inhibit_rpath = true;
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
		inhibit_rpath = true;
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
#endif
					"/lib64:/usr/lib64:/usr/local/lib64"
					":/lib:/usr/lib:/usr/local/lib");
	if (status < 0)
		return 0; /* Not fatal.  */

	status = find_item(envp, "LD_LIBRARY_PATH");
	if (status < 0)
		return 0; /* Not fatal.  */
	index = (size_t) status;

	/* Allocate a new entry at the end of envp[] if
	 * LD_LIBRARY_PATH was not found.  */
	if (index == envp->length) {
		index = (envp->length > 0 ? envp->length - 1 : 0);
		status = resize_array(envp, index, 1);
		if (status < 0)
			return 0; /* Not fatal.  */
	}

	/* Forge the new LD_LIBRARY_PATH variable from
	 * host_ldso_paths.  */
	length1 = strlen("LD_LIBRARY_PATH=");
	length2 = strlen(host_ldso_paths);
	if (ARG_MAX - length2 - 1 < length1)
		return 0; /* Not fatal.  */

	memmove(host_ldso_paths + length1, host_ldso_paths, length2 + 1);
	memcpy(host_ldso_paths, "LD_LIBRARY_PATH=" , length1);

	write_item(envp, index, host_ldso_paths);

	return (int) inhibit_rpath;
}
