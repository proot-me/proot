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

#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h> /* pid_t, size_t, */
#include <sys/user.h>  /* struct user*, */
#include <stdbool.h>   /* bool,  */
#include <sys/queue.h> /* LIST_*, */
#include <sys/ptrace.h>/* enum __ptrace_request */
#include <talloc.h>    /* talloc_*, */
#include <stdint.h>    /* *int*_t, */

#include "arch.h" /* word_t, user_regs_struct, */
#include "compat.h"

typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;

struct bindings;
struct load_info;
struct extensions;
struct chained_syscalls;

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

/* Virtual heap, emulated with a regular memory mapping.  */
typedef struct {
	word_t base;
	size_t size;
	bool disabled;
} Heap;

/* Information related to a tracee process. */
typedef struct tracee {
	/**********************************************************************
	 * Private resources                                                  *
	 **********************************************************************/

	/* Link for the list of all tracees.  */
	LIST_ENTRY(tracee) link;

	/* Process identifier. */
	pid_t pid;

	/* Unique tracee identifier. */
	uint64_t vpid;

	/* Is it currently running or not?  */
	bool running;

	/* Is this tracee ready to be freed?  TODO: move to a list
	 * dedicated to terminated tracees instead.  */
	bool terminated;

        /* Whether termination of this tracee implies an immediate kill
         * of all tracees. */
        bool killall_on_exit;

	/* Parent of this tracee, NULL if none.  */
	struct tracee *parent;

	/* Is it a "clone", i.e has the same parent as its creator.  */
	bool clone;

	/* Support for ptrace emulation (tracer side).  */
	struct {
		size_t nb_ptracees;
		LIST_HEAD(zombies, tracee) zombies;

		pid_t wait_pid;
		word_t wait_options;

		enum {
			DOESNT_WAIT = 0,
			WAITS_IN_KERNEL,
			WAITS_IN_PROOT
		} waits_in;
	} as_ptracer;

	/* Support for ptrace emulation (tracee side).  */
	struct {
		struct tracee *ptracer;

		struct {
			#define STRUCT_EVENT struct { int value; bool pending; }

			STRUCT_EVENT proot;
			STRUCT_EVENT ptracer;
		} event4;

		bool tracing_started;
		bool ignore_loader_syscalls;
		bool ignore_syscalls;
		word_t options;
		bool is_zombie;
	} as_ptracee;

	/* Current status:
	 *        0: enter syscall
	 *        1: exit syscall no error
	 *   -errno: exit syscall with error.  */
	int status;

#define IS_IN_SYSENTER(tracee) ((tracee)->status == 0)
#define IS_IN_SYSEXIT(tracee) (!IS_IN_SYSENTER(tracee))
#define IS_IN_SYSEXIT2(tracee, sysnum) (IS_IN_SYSEXIT(tracee) \
				     && get_sysnum((tracee), ORIGINAL) == sysnum)

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

	/* Context used to collect all dynamic memory allocations that
	 * should be released once this tracee is freed.  */
	TALLOC_CTX *life_context;

	/* Note: I could rename "ctx" in "event_span" and
	 * "life_context" in "life_span".  */

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

	/* Unrequested syscalls inserted by PRoot after an actual
	 * syscall.  */
	struct {
		struct chained_syscalls *syscalls;
		bool force_final_result;
		word_t final_result;
	} chain;

	/* Load info generated during execve sysenter and used during
	 * execve sysexit.  */
	struct load_info *load_info;


	/**********************************************************************
	 * Private but inherited resources                                    *
	 **********************************************************************/

	/* Verbose level.  */
	int verbose;

	/* State of the seccomp acceleration for this tracee.  */
	enum { DISABLED = 0, DISABLING, ENABLED } seccomp;

	/* Ensure the sysexit stage is always hit under seccomp.  */
	bool sysexit_pending;


	/**********************************************************************
	 * Shared or private resources, depending on the CLONE_FS/VM flags.   *
	 **********************************************************************/

	/* Information related to a file-system name-space.  */
	FileSystemNameSpace *fs;

	/* Virtual heap, emulated with a regular memory mapping.  */
	Heap *heap;


	/**********************************************************************
	 * Shared resources until the tracee makes a call to execve().        *
	 **********************************************************************/

	/* Path to the executable, à la /proc/self/exe.  */
	char *exe;
	char *new_exe;


	/**********************************************************************
	 * Shared or private resources, depending on the (re-)configuration   *
	 **********************************************************************/

	/* Runner command-line.  */
	char **qemu;

	/* Path to glue between the guest rootfs and the host rootfs.  */
	const char *glue;

	/* List of extensions enabled for this tracee.  */
	struct extensions *extensions;


	/**********************************************************************
	 * Shared but read-only resources                                     *
	 **********************************************************************/

	/* For the mixed-mode, the guest LD_LIBRARY_PATH is saved
	 * during the "guest -> host" transition, in order to be
	 * restored during the "host -> guest" transition (only if the
	 * host LD_LIBRARY_PATH hasn't changed).  */
	const char *host_ldso_paths;
	const char *guest_ldso_paths;

	/* For diagnostic purpose.  */
	const char *tool_name;

} Tracee;

#define HOST_ROOTFS "/host-rootfs"

#define TRACEE(a) talloc_get_type_abort(talloc_parent(talloc_parent(a)), Tracee)

extern Tracee *get_tracee(const Tracee *tracee, pid_t pid, bool create);
extern Tracee *get_stopped_ptracee(const Tracee *ptracer, pid_t pid,
				bool only_with_pevent, word_t wait_options);
extern bool has_ptracees(const Tracee *ptracer, pid_t pid, word_t wait_options);
extern int new_child(Tracee *parent, word_t clone_flags);
extern Tracee *new_dummy_tracee(TALLOC_CTX *context);
extern void terminate_tracee(Tracee *tracee);
extern void free_terminated_tracees();
extern int swap_config(Tracee *tracee1, Tracee *tracee2);
extern void kill_all_tracees();

#endif /* TRACEE_H */
