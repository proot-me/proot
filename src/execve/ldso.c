/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 * Inspired by: ld.so(8) from the GNU C Library.
 */

#include <stdbool.h> /* bool, true, false, */
#include <string.h>  /* strlen(3), strcpy(3), strcat(3), strcmp(3), */
#include <stdlib.h>  /* getenv(3), calloc(3), */
#include <assert.h>  /* assert(3), */
#include <errno.h>   /* ENOMEM, */

#include "execve/ldso.h"
#include "execve/execve.h"
#include "execve/args.h"
#include "notice.h"
#include "config.h"

/* The value of LD_LIBRARY_PATH is saved at initialization time to
 * ensure host programs (the runner for instance) will not be affected
 * by the future values of LD_LIBRARY_PATH as defined in the guest
 * environment.  */
static char *host_ldso_path = NULL;

void init_module_ldso()
{
	host_ldso_path = getenv("LD_LIBRARY_PATH");
	if (!host_ldso_path)
		return;

	host_ldso_path = strdup(host_ldso_path);
	if (!host_ldso_path)  {
		notice(WARNING, SYSTEM, "can't allocate memory");
		return;
	}

	/* TODO: do the same for LD_PRELOAD?  */
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

		bool passthru(const char *variable, const char *value) {
			size_t length = strlen(variable);

			if ((*envp)[i][length] != '='
			    || strncmp((*envp)[i], variable, length) != 0)
				return false;

			/* Errors are not fatal here.  */
			push_args(false, argv, 2, define, (*envp)[i]);
			replace_env_entry(&(*envp)[i], value);
			return true;
		}

		has_seen_library_path |= passthru("LD_LIBRARY_PATH", host_ldso_path);
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
		new_env_entry(envp, "LD_LIBRARY_PATH", host_ldso_path);
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
