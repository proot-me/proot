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

#include <assert.h>  /* assert(3), */
#include <stdint.h>  /* intptr_t, */
#include <errno.h>   /* E*, */
#include <sys/stat.h>   /* chmod(2), stat(2) */

#include "extension/extension.h"
#include "syscall/syscall.h"
#include "tracee/tracee.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "tracee/reg.h"
#include "path/path.h"
#include "path/binding.h"

typedef struct ModifiedNode {
	SLIST_ENTRY(ModifiedNode) link;
	char *path;
	mode_t old_mode;
} ModifiedNode;

typedef SLIST_HEAD(ModifiedNodes, ModifiedNode) ModifiedNodes;

typedef struct {
	int interesting_syscall;
	ModifiedNodes *modified_nodes;
} Config;


/**
 * Free the list of ModifiedNode and undo the modifications
 */
static int modified_nodes_free(ModifiedNodes *nodes_list) {
	/* Empty the list of nodes the last added first */
	while (!SLIST_EMPTY(nodes_list)) {
		ModifiedNode *node = SLIST_FIRST(nodes_list);
		SLIST_REMOVE_HEAD(nodes_list, link);

		/* Undo the modifications */
		chmod(node->path, node->old_mode);
		talloc_free(node);
	}

	return 0;
}


/**
 * Handler for this @extension.  It is triggered each time an @event
 * occured.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int fake_id0_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	switch (event) {
	/* Create the config structure for each new tracee */
	case INITIALIZATION:
	case INHERIT_CHILD: {
		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;
		return 0;
	}

	/* The extension if attached to every new tracee with a new configuration */
	case INHERIT_PARENT:
		return 1;

	case HOST_PATH: {
		Tracee *tracee = TRACEE(extension);

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/fake_id0/host_path.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/fake_id0/host_path.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
	}

	case SYSCALL_ENTER_START: {
		Tracee *tracee = TRACEE(extension);

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/fake_id0/enter.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/fake_id0/enter.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
	}

	case SYSCALL_EXIT_END: {
		Tracee *tracee = TRACEE(extension);

		switch (get_abi(tracee)) {
		case ABI_DEFAULT: {
			#include SYSNUM_HEADER
			#include "extension/fake_id0/exit.c"
			return 0;
		}
		#ifdef SYSNUM_HEADER2
		case ABI_2: {
			#include SYSNUM_HEADER2
			#include "extension/fake_id0/exit.c"
			return 0;
		}
		#endif
		default:
			assert(0);
		}
		#include "syscall/sysnum-undefined.h"
	}

	default:
		return 0;
	}
}
