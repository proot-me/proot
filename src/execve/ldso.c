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
#include <limits.h>  /* PATH_MAX, */
#include <unistd.h>  /* close(2), */

#include "execve/ldso.h"
#include "execve/execve.h"
#include "execve/args.h"
#include "execve/elf.h"
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
 * Note that the variables LD_LIBRARY_PATH and LD_PRELOAD are always
 * needed by QEMU (it should be harmless with another runner):
 *
 *     env LD_PRELOAD=libgcc_s.so.1 qemu -U LD_PRELOAD /bin/ls
 *
 * or when LD_PRELOAD was also specified by the user:
 *
 *     env LD_PRELOAD=libgcc_s.so.1 qemu -E LD_PRELOAD=libm.so.6 /bin/ls
 */
bool ldso_env_passthru(char **envp[], char **argv[], const char *define, const char *undefine)
{
	int i;
	bool has_seen_library_path = false;
	bool has_seen_preload = false;

	for (i = 0; (*envp)[i] != NULL; i++) {
		bool is_known = false;

		/* Skip variables that do not start with "LD_".  */
		if (   (*envp)[i][0] != 'L'
		    || (*envp)[i][1] != 'D'
		    || (*envp)[i][2] != '_')
			continue;

		bool passthru(const char *name, const char *value) {
			if (!check_env_entry_name((*envp)[i], name))
				return false;

			/* Errors are not fatal here.  */
			push_args(false, argv, 2, define, (*envp)[i]);
			replace_env_entry(&(*envp)[i], value);
			return true;
		}

		has_seen_library_path |= passthru("LD_LIBRARY_PATH", initial_ldso_paths);
		has_seen_preload      |= passthru("LD_PRELOAD", "libgcc_s.so.1");

		is_known |= passthru("LD_BIND_NOW", NULL);
		is_known |= passthru("LD_TRACE_LOADED_OBJECTS", NULL);
		is_known |= passthru("LD_AOUT_LIBRARY_PATH", NULL);
		is_known |= passthru("LD_AOUT_PRELOAD", NULL);
		is_known |= passthru("LD_AUDIT", NULL);
		is_known |= passthru("LD_BIND_NOT", NULL);
		is_known |= passthru("LD_DEBUG", NULL);
		is_known |= passthru("LD_DEBUG_OUTPUT", NULL);
		is_known |= passthru("LD_DYNAMIC_WEAK", NULL);
		is_known |= passthru("LD_HWCAP_MASK", NULL);
		is_known |= passthru("LD_KEEPDIR", NULL);
		is_known |= passthru("LD_NOWARN", NULL);
		is_known |= passthru("LD_ORIGIN_PATH", NULL);
		is_known |= passthru("LD_POINTER_GUARD", NULL);
		is_known |= passthru("LD_PROFILE", NULL);
		is_known |= passthru("LD_PROFILE_OUTPUT", NULL);
		is_known |= passthru("LD_SHOW_AUXV", NULL);
		is_known |= passthru("LD_USE_LOAD_BIAS", NULL);
		is_known |= passthru("LD_VERBOSE", NULL);
		is_known |= passthru("LD_WARN", NULL);

		if (!is_known && config.verbose_level >= 1)
			notice(WARNING, INTERNAL, "unknown LD_ environment variable");
	}

	if (!has_seen_library_path) {
		/* Errors are not fatal here.  */
		new_env_entry(envp, "LD_LIBRARY_PATH", initial_ldso_paths);
		push_args(false, argv, 2, undefine, "LD_LIBRARY_PATH");
	}

	if (!has_seen_preload) {
		/* Errors are not fatal here.  */
		new_env_entry(envp, "LD_PRELOAD", "libgcc_s.so.1");
		push_args(false, argv, 2, undefine, "LD_PRELOAD");
	}

	/* Return always true since LD_LIBRARY_PATH and LD_PRELOAD are
	 * always changed.  */
	return true;
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
int rebuild_host_ldso_paths(const char t_program[PATH_MAX], char **envp[])
{
	union elf_header elf_header;

	char host_ldso_paths[ARG_MAX] = "";
	bool inhibit_rpath = false;

	char **env_entry = NULL;
	char *rpaths   = NULL;
	char *runpaths = NULL;

	int status;
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

	/* Search if there's a LD_LIBRARY_PATH variable. */
	env_entry = get_env_entry(*envp, "LD_LIBRARY_PATH");

	/* Errors are not fatal here either.  */
	if (env_entry != NULL)
		replace_env_entry(env_entry, host_ldso_paths);
	else
		new_env_entry(envp, "LD_LIBRARY_PATH", host_ldso_paths);

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
