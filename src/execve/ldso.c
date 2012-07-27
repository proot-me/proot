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

#include <stdbool.h> /* bool, true, false, */
#include <string.h>  /* strlen(3), strcpy(3), strcat(3), strcmp(3), */
#include <stdlib.h>  /* getenv(3), calloc(3), */
#include <assert.h>  /* assert(3), */
#include <errno.h>   /* ENOMEM, */
#include <unistd.h>  /* close(2), */
#include <linux/limits.h>  /* PATH_MAX, ARG_MAX, */

#include "execve/ldso.h"
#include "execve/execve.h"
#include "execve/elf.h"
#include "tracee/array.h"
#include "notice.h"
#include "config.h"

/* The value of LD_LIBRARY_PATH is saved at initialization time to
 * ensure host programs (the runner for instance) will not be affected
 * by the future values of LD_LIBRARY_PATH as defined in the guest
 * environment.  */
static char *initial_ldso_paths = NULL;

/* TODO: do the same for LD_PRELOAD?  */

void init_module_ldso()
{
	initial_ldso_paths = getenv("LD_LIBRARY_PATH");
	if (!initial_ldso_paths)
		return;

	initial_ldso_paths = strdup(initial_ldso_paths);
	if (!initial_ldso_paths)  {
		notice(WARNING, SYSTEM, "can't allocate memory");
		return;
	}
}

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
int compare_item_env(struct array *array, size_t index, const char *reference)
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
int ldso_env_passthru(struct array *envp, struct array *argv,
		      const char *define, const char *undefine)
{
	bool has_seen_library_path = false;
	int status;
	int i;

	for (i = 0; i < envp->length; i++) {
		bool is_known = false;
		char *env;

		status = read_item_string(envp, i, &env);
		if (status < 0)
			return status;

		/* Skip variables that do not start with "LD_".  */
		if (env == NULL || strncmp(env, "LD_", sizeof("LD_") - 1) != 0)
			continue;

		bool passthru(const char *name) {
			if (!is_env_name(env, name))
				return false;

			/* Errors are not fatal here.  */
			status = resize_array(argv, 1, 2);
			if (status >= 0) {
				status = write_items(argv, 1, 2, define, env);
				/* TODO: should be fatal here.  */
			}
			write_item(envp, i, "");
			return true;
		}

		has_seen_library_path |= passthru("LD_LIBRARY_PATH");
		is_known |= passthru("LD_PRELOAD");
		is_known |= passthru("LD_BIND_NOW");
		is_known |= passthru("LD_TRACE_LOADED_OBJECTS");
		is_known |= passthru("LD_AOUT_LIBRARY_PATH");
		is_known |= passthru("LD_AOUT_PRELOAD");
		is_known |= passthru("LD_AUDIT");
		is_known |= passthru("LD_BIND_NOT");
		is_known |= passthru("LD_DEBUG");
		is_known |= passthru("LD_DEBUG_OUTPUT");
		is_known |= passthru("LD_DYNAMIC_WEAK");
		is_known |= passthru("LD_HWCAP_MASK");
		is_known |= passthru("LD_KEEPDIR");
		is_known |= passthru("LD_NOWARN");
		is_known |= passthru("LD_ORIGIN_PATH");
		is_known |= passthru("LD_POINTER_GUARD");
		is_known |= passthru("LD_PROFILE");
		is_known |= passthru("LD_PROFILE_OUTPUT");
		is_known |= passthru("LD_SHOW_AUXV");
		is_known |= passthru("LD_USE_LOAD_BIAS");
		is_known |= passthru("LD_VERBOSE");
		is_known |= passthru("LD_WARN");

		if (!is_known && config.verbose_level >= 1)
			notice(WARNING, INTERNAL, "unknown LD_ environment variable");
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
			length1 += strlen(config.host_rootfs);

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
		 * as "${host_rootfs}/lib" to avoid conflict with the
		 * guest "/lib".  */
		if (is_absolute) {
			strcpy(cursor1, config.host_rootfs);
			cursor1 += strlen(config.host_rootfs);
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
int rebuild_host_ldso_paths(const char t_program[PATH_MAX], struct array *envp)
{
	union elf_header elf_header;

	char host_ldso_paths[ARG_MAX] = "";
	bool inhibit_rpath = false;

	char *rpaths   = NULL;
	char *runpaths = NULL;

	size_t length1;
	size_t length2;

	int status;
	int index;
	int fd;

	fd = open_elf(t_program, &elf_header);
	if (fd < 0)
		return fd;

	status = read_ldso_rpaths(fd, &elf_header, &rpaths, &runpaths);
	if (status < 0)
		goto end;

	/* 1. DT_RPATH  */
	if (rpaths && !runpaths) {
		status = add_host_ldso_paths(host_ldso_paths, rpaths);
		if (status < 0) {
			status = 0; /* Not fatal.  */
			goto end;
		}
		inhibit_rpath = true;
	}


	/* 2. LD_LIBRARY_PATH  */
	if (initial_ldso_paths) {
		status = add_host_ldso_paths(host_ldso_paths, initial_ldso_paths);
		if (status < 0) {
			status = 0; /* Not fatal.  */
			goto end;
		}
	}

	/* 3. DT_RUNPATH  */
	if (runpaths) {
		status = add_host_ldso_paths(host_ldso_paths, runpaths);
		if (status < 0) {
			status = 0; /* Not fatal.  */
			goto end;
		}
		inhibit_rpath = true;
	}

	/* 4. /etc/ld.so.cache NYI.  */

	/* 5. /lib[32|64], /usr/lib[32|64] + /usr/local/lib[32|64]  */
	/* 6. /lib, /usr/lib + /usr/local/lib  */
	if (IS_CLASS32(elf_header))
		status = add_host_ldso_paths(host_ldso_paths,
					"/lib32:/usr/lib32:/usr/local/lib32"
					":/lib:/usr/lib:/usr/local/lib");
	else
		status = add_host_ldso_paths(host_ldso_paths,
					"/lib64:/usr/lib64:/usr/local/lib64"
					":/lib:/usr/lib:/usr/local/lib");
	if (status < 0) {
		status = 0; /* Not fatal.  */
		goto end;
	}

	index = find_item(envp, "LD_LIBRARY_PATH");
	if (index < 0) {
		status = 0; /* Not fatal.  */
		goto end;
	}
	/* Allocate a new entry at the end of envp[] if
	 * LD_LIBRARY_PATH was not found.  */
	if (index == envp->length) {
		index = envp->length - 1;
		resize_array(envp, envp->length - 1, 1);
	}

	/* Forge the new LD_LIBRARY_PATH variable from
	 * host_ldso_paths.  */
	length1 = strlen("LD_LIBRARY_PATH=");
	length2 = strlen(host_ldso_paths);
	if (ARG_MAX - length2 - 1 < length1) {
		status = 0; /* Not fatal.  */
		goto end;
	}
	memmove(host_ldso_paths + length1, host_ldso_paths, length2 + 1);
	memcpy(host_ldso_paths, "LD_LIBRARY_PATH=" , length1);

	write_item(envp, index, host_ldso_paths);
	status = 0;

end:
	close(fd);

	if (rpaths)
		free(rpaths);

	if (runpaths)
		free(runpaths);

	/* Delayed error handling.  */
	if (status < 0)
		return status;

	return (int) inhibit_rpath;
}
