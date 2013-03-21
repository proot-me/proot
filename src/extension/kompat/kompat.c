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

#include <stdint.h>        /* intptr_t, */
#include <stdlib.h>        /* strtoul(3), */
#include <linux/version.h> /* KERNEL_VERSION, */
#include <assert.h>        /* assert(3), */
#include <sys/utsname.h>   /* uname(2), utsname, */
#include <string.h>        /* strncpy(3), */
#include <talloc.h>        /* talloc_*, */
#include <fcntl.h>         /* AT_*,  */

#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "tracee/mem.h"

#include "attribute.h"
#include "compat.h"

#define MAX_ARG_SHIFT 2
typedef struct {
	int expected_release;
	word_t new_sysarg_num;
	struct {
		Reg sysarg;     /* first argument to be moved.  */
		size_t nb_args; /* number of arguments to be moved.  */
		int offset;     /* offset to be applied.  */
	} shifts[MAX_ARG_SHIFT];
} Modif;

typedef struct {
	const char *release;
	int emulated_release;
	int actual_release;
} Config;

/**
 * Modify the current syscall of @tracee as described by @modif
 * regarding the given @config.
 */
static void modify_syscall(Tracee *tracee, const Config *config, const Modif *modif)
{
	size_t i, j;

	assert(config != NULL);

	/* Emulate only syscalls that are available in the expected
	 * release but that are missing in the actual one.  */
	if (   modif->expected_release <= config->actual_release
	    || modif->expected_release > config->emulated_release)
		return;

	poke_reg(tracee, SYSARG_NUM, modif->new_sysarg_num);

	/* Shift syscall arguments.  */
	for (i = 0; i < MAX_ARG_SHIFT; i++) {
		Reg sysarg     = modif->shifts[i].sysarg;
		size_t nb_args = modif->shifts[i].nb_args;
		int offset     = modif->shifts[i].offset;

		for (j = 0; j < nb_args; j++) {
			word_t arg = peek_reg(tracee, CURRENT, sysarg + j);
			poke_reg(tracee, sysarg + j + offset, arg);
		}
	}

	return;
}

/**
 * Return the numeric value for the given kernel @release.
 */
static int parse_kernel_release(const char *release)
{
	unsigned long major = 0;
	unsigned long minor = 0;
	unsigned long revision = 0;
	char *cursor = (char *)release;

	major = strtoul(cursor, &cursor, 10);

	if (*cursor == '.') {
		cursor++;
		minor = strtoul(cursor, &cursor, 10);
	}

	if (*cursor == '.') {
		cursor++;
		revision = strtoul(cursor, &cursor, 10);
	}

	return KERNEL_VERSION(major, minor, revision);
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occured.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int kompat_callback(Extension *extension, ExtensionEvent event,
		intptr_t data1, intptr_t data2 UNUSED)
{
	int status;

	switch (event) {
	case INITIALIZATION: {
		struct utsname utsname;
		Config *config;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		config = extension->config;

		config->release = talloc_strdup(config, (const char *)data1);
		if (config->release == NULL)
			return -1;
		talloc_set_name_const(config->release, "$release");

		config->emulated_release = parse_kernel_release(config->release);

		status = uname(&utsname);
		if (status < 0 || getenv("PROOT_FORCE_SYSCALL_COMPAT") != NULL)
			config->actual_release = 0;
		else
			config->actual_release = parse_kernel_release(utsname.release);

		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		/* Nothing to do if this syscall is being discarded
		 * (because of an error detected by PRoot).  */
		if ((int) data1 < 0)
			return 0;

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/kompat/enter.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/kompat/enter.c"
			return 0;
		}
		#endif
		#ifdef SYSNUM_HEADER3
		case ABI_3: {
			#include SYSNUM_HEADER3
			#include "extension/kompat/enter.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
		return 0;
	}

	case SYSCALL_EXIT_END: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/kompat/exit.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/kompat/exit.c"
			return 0;
		}
		#endif
		#ifdef SYSNUM_HEADER3
		case ABI_3: {
			#include SYSNUM_HEADER3
			#include "extension/kompat/exit.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
		return 0;
	}

	default:
		return 0;
	}
}

