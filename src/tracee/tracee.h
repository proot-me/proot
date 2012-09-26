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

#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h> /* pid_t, size_t, */
#include <sys/user.h>  /* struct user*, */
#include <stdbool.h>
#include <sys/queue.h> /* LIST_*, */

#include <sys/queue.h> /* LIST_*, */
#include <talloc.h>    /* talloc_*, */

#include "arch.h" /* word_t, user_regs_struct, */

typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;

struct bindings;

/* Information related to a tracee process. */
typedef struct tracee {
	/**********************************************************************
	 * Private resources                                                  *
	 **********************************************************************/

	/* Link for the list of all tracees.  */
	LIST_ENTRY(tracee) link;

	/* Process identifier. */
	pid_t pid;

	/* Current status:
	 *        0: enter syscall
	 *        1: exit syscall no error
	 *   -errno: exit syscall with error.  */
	int status;

	/* Value of the tracee's general purpose registers.  */
	struct user_regs_struct _regs[NB_REG_VERSION];
	bool _regs_have_changed;

	/* State for the special handling of SIGSTOP.  */
	enum {
		SIGSTOP_IGNORED = 0,  /* Ignore SIGSTOP (once the parent is known).  */
		SIGSTOP_ALLOWED,      /* Allow SIGSTOP (once the parent is known).   */
		SIGSTOP_PENDING,      /* Block SIGSTOP until the parent is unknown.  */
	} sigstop;

	/* Context used to collect all the temporary memory required
	 * during the translation of a syscall (enter and exit
	 * stages).  */
	TALLOC_CTX *tmp;

	/* Specify the type of the final component during the
	 * initialization of a binding.  This variable is first
	 * defined in bind_path() then used in build_glue().  */
	mode_t glue_type;


	/***********************************************************************
	 * Shared resources until the tracee changes its filesystem namespace. *
	 ***********************************************************************/

	/* List of bindings as specified by the user.  */
	struct bindings *bindings_user;

	/* List of bindings canonicalized and sorted in the "guest" order.  */
	struct bindings *bindings_guest;

	/* List of bindings canonicalized and sorted in the "host" order.  */
	struct bindings *bindings_host;


	/**********************************************************************
	 * Shared resources until the tracee makes a call to execve().       *
	 **********************************************************************/

	/* Path to the executable, à la /proc/self/exe.  */
	char *exe;

	/* Initial command-line, à la /proc/self/cmdline.  */
	char **cmdline;


	/**********************************************************************
	 * Shared or private resources, depending on the CLONE_FS flag.       *
	 **********************************************************************/

	/* Current working directory, à la /proc/self/pwd.  */
	char *cwd;

	/* Path to the root file-system, à la /proc/self/root.  */
	char *root;


	/**********************************************************************
	 * Shared or private resources, depending on the (re-)configuration   *
	 **********************************************************************/

	/* Runner command-line.  */
	char **qemu;

	/* Path to glue between the guest rootfs and the host rootfs.  */
	char *glue;

	/* Extra configuration (will be handled by dedicated extensions soon).  */
	const char *kernel_release;
	bool fake_id0;
} Tracee;

#define HOST_ROOTFS "/host-rootfs"

#define TRACEE(a) talloc_get_type_abort(talloc_parent(talloc_parent(a)), Tracee)

extern Tracee *get_tracee(pid_t pid, bool create);
extern void inherit(Tracee *child, Tracee *parent);
extern void kill_all_tracees();

#endif /* TRACEE_H */
