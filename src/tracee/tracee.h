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

#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h> /* pid_t, size_t, */
#include <sys/user.h>  /* struct user*, */
#include <stdbool.h>
#include <sys/queue.h> /* LIST_*, */
#include <sys/ptrace.h>/* enum __ptrace_request */
#include <talloc.h>    /* talloc_*, */

#include "arch.h" /* word_t, user_regs_struct, */
#include "compat.h"

typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;

struct bindings;
struct extensions;

/* Information related to a file-system name-space.  */
typedef struct {
	struct {
	     /* List of bindings as specified by the user but not canonicalized yet.  */
		struct bindings *pending;

		/* List of bindings canonicalized and sorted in the "guest" order.  */
		struct bindings *guest;

		/* List of bindings canonicalized and sorted in the "host" order.  */
		struct bindings *host;
	} bindings;

	/* Current working directory, à la /proc/self/pwd.  */
	char *cwd;
} FileSystemNameSpace;

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

	/* How this tracee is restarted.  */
	enum __ptrace_request restart_how;

	/* Value of the tracee's general purpose registers.  */
	struct user_regs_struct _regs[NB_REG_VERSION];
	bool _regs_were_changed;
	bool restore_original_regs;

	/* State for the special handling of SIGSTOP.  */
	enum {
		SIGSTOP_IGNORED = 0,  /* Ignore SIGSTOP (once the parent is known).  */
		SIGSTOP_ALLOWED,      /* Allow SIGSTOP (once the parent is known).   */
		SIGSTOP_PENDING,      /* Block SIGSTOP until the parent is unknown.  */
	} sigstop;

	/* Context used to collect all the temporary dynamic memory
	 * allocations.  */
	TALLOC_CTX *ctx;

	/* Specify the type of the final component during the
	 * initialization of a binding.  This variable is first
	 * defined in bind_path() then used in build_glue().  */
	mode_t glue_type;

	/* During a sub-reconfiguration, the new setup is relatively
	 * to @tracee's file-system name-space.  Also, @paths holds
	 * its $PATH environment variable in order to emulate the
	 * execvp(3) behavior.  */
	struct {
		struct tracee *tracee;
		const char *paths;
	} reconf;


	/**********************************************************************
	 * Shared or private resources, depending on the CLONE_FS flag.       *
	 **********************************************************************/

	FileSystemNameSpace *fs;


	/**********************************************************************
	 * Shared resources until the tracee makes a call to execve().        *
	 **********************************************************************/

	/* Path to the executable, à la /proc/self/exe.  */
	char *exe;

	/* Initial command-line, à la /proc/self/cmdline.  */
	char **cmdline;


	/**********************************************************************
	 * Shared or private resources, depending on the (re-)configuration   *
	 **********************************************************************/

	/* Runner command-line.  */
	char **qemu;

	/* Can the ELF interpreter for QEMU be safely skipped?  */
	bool qemu_pie_workaround;

	/* Path to glue between the guest rootfs and the host rootfs.  */
	char *glue;

	/* List of extensions enabled for this tracee.  */
	struct extensions *extensions;


	/**********************************************************************
	 * Private resources                                                  *
	 **********************************************************************/

	/* Verbose level.  */
	int verbose;

} Tracee;

#define HOST_ROOTFS "/host-rootfs"

#define TRACEE(a) talloc_get_type_abort(talloc_parent(talloc_parent(a)), Tracee)

extern Tracee *get_tracee(const Tracee *tracee, pid_t pid, bool create);
extern int new_child(Tracee *parent, word_t clone_flags);
extern int swap_config(Tracee *tracee1, Tracee *tracee2);
extern int parse_config(Tracee *tracee, size_t argc, char *argv[]);
extern void kill_all_tracees();

#endif /* TRACEE_H */
