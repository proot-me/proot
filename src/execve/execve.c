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

#include <sys/stat.h> /* lstat(2), S_ISREG(), */
#include <unistd.h>   /* access(2), lstat(2), */
#include <string.h>   /* string(3), */
#include <stdlib.h>   /* realpath(3), getenv(3),  */
#include <assert.h>   /* assert(3), */
#include <errno.h>    /* E*, */

#include "execve/execve.h"
#include "execve/args.h"
#include "execve/interp.h"
#include "execve/elf.h"
#include "execve/ldso.h"
#include "notice.h"
#include "path/path.h"
#include "config.h"

#include "compat.h"

/**
 * Translate @u_path into @t_path and check if this latter exists, is
 * executable and is a regular file.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
static int translate_n_check(struct tracee_info *tracee, char t_path[PATH_MAX], const char *u_path)
{
	struct stat statl;
	int status;

	status = translate_path(tracee, t_path, AT_FDCWD, u_path, REGULAR);
	if (status < 0)
		return status;

	status = access(t_path, F_OK);
	if (status < 0)
		return -ENOENT;

	status = access(t_path, X_OK);
	if (status < 0)
		return -EACCES;

	status = lstat(t_path, &statl);
#ifdef BENCHMARK_TRACEE_HANDLING
	if (status < 0 || !S_ISREG(statl.st_mode))
		return -EPERM;
#else
	if (status < 0)
		return -EPERM;
#endif

	return 0;
}

/**
 * Substitute *@argv[0] with the interpreter (and its argument) of the
 * program pointed to by @u_path. The paths to the interpreter (or to
 * the program itself if it doesn't use an interpreter) are stored in
 * @t_interp and @u_interp (respectively translated and untranslated).
 */
static int expand_interp(struct tracee_info *tracee,
			 const char *u_path,
			 char t_interp[PATH_MAX],
			 char u_interp[PATH_MAX],
			 char **argv[],
			 extract_interp_t callback)
{
	char argument[ARG_MAX];

	char dummy_path[PATH_MAX];
	char dummy_arg[ARG_MAX];

	int status;

	status = translate_n_check(tracee, t_interp, u_path);
	if (status < 0)
		return status;

	/* Extract the interpreter of t_interp in u_interp + argument. */
	status = callback(tracee, t_interp, u_interp, argument);
	if (status < 0)
		return status;

	/* No interpreter was found, in this case we execute the
	 * translation of u_path (t_interp) directly. */
	if (status == 0) {
		strcpy(u_interp, u_path);
		return 0;
	}

	status = translate_n_check(tracee, t_interp, u_interp);
	if (status < 0)
		return status;

	/* Sanity check: an interpreter doesn't request an[other]
	 * interpreter on Linux.  In the case of ELF this check
	 * ensures the interpreter is in the ELF format.  Note
	 * 'u_interp' and 'argument' are actually not used. */
	status = callback(tracee, t_interp, dummy_path, dummy_arg);
	if (status < 0)
		return status;
	if (status != 0)
		return -EPERM;

	/*
	 * Substitute argv[0] with the ELF/script interpreter:
	 *
	 *     execve("/bin/sh", { "/bin/sh", "/test.sh", NULL }, { NULL });
	 *
	 * becomes:
	 *
	 *     execve("/lib/ld.so", { "/lib/ld.so", "/bin/sh", "/test.sh", NULL }, { NULL });
	 *
	 * Note: actually the first parameter of execve() is changed
	 * in the caller because it depends on the use of a runner.
	 */

	VERBOSE(3, "expand shebang: %s -> %s %s %s",
		(*argv)[0], u_path, argument, u_path);

	if (argument[0] != '\0')
		status = push_args(true, argv, 3, u_interp, argument, u_path);
	else
		status = push_args(true, argv, 2, u_interp, u_path);
	if (status < 0)
		return status;

	/* Remember at this point t_interp is the translation of
	 * u_interp. */
	return 1;
}

/**
 * Translate the arguments of the execve() syscall made by the @tracee
 * process. This syscall needs a very special treatment for script
 * files because according to "man 2 execve":
 *
 *     An interpreter script is a text file [...] whose first line is
 *     of the form:
 *
 *         #! interpreter [optional-arg]
 *
 *     The interpreter must be a valid pathname for an executable
 *     which is not itself a script.  If the filename argument of
 *     execve() specifies an interpreter script, then interpreter will
 *     be invoked with the following arguments:
 *
 *         interpreter [optional-arg] filename arg...
 *
 *     where arg...  is the series of words pointed to by the argv
 *     argument of execve().
 *
 * Let's take the following example:
 *
 *     execve("/bin/script.sh", argv = [ "script.sh", "arg1", arg2", ... ], envp);
 *
 * We can't just translate the first parameter because the kernel
 * will actually run the interpreter "/bin/sh" with the translated
 * path to the script file "/tmp/new_root/bin/script.sh" as its first
 * argument. Technically, we want the opposite behaviour, that is, we
 * want to run the translated path to the interpreter
 * "/tmp/new_root/bin/sh" with the de-translated path to the script
 * "/bin/script.sh" as its first parameter (will be translated later):
 *
 *     execve("/tmp/new_root/bin/sh", argv = [ "/bin/sh", "/bin/script.sh", "arg1", arg2", ... ], envp);
 */
int translate_execve(struct tracee_info *tracee)
{
	char u_path[PATH_MAX];
	char t_interp[PATH_MAX];
	char u_interp[PATH_MAX];

	char **envp = NULL;
	char **argv = NULL;
	char *argv0 = NULL;

	bool envp_has_changed = false;
	bool argv_has_changed = false;
	int size = 0;
	int status;

	status = get_sysarg_path(tracee, u_path, SYSARG_1);
	if (status < 0)
		return status;

	status = get_args(tracee, &argv, SYSARG_2);
	if (status < 0)
		goto end;
	assert(argv != NULL);

	if(config.qemu)
		argv0 = strdup(argv[0]);

	status = get_args(tracee, &envp, SYSARG_3);
	if (status < 0)
		goto end;
	assert(envp != NULL);

	status = expand_interp(tracee, u_path, t_interp, u_interp, &argv, extract_script_interp);
	if (status < 0)
		goto end;
	argv_has_changed = (status > 0);

	/* I'd prefer the binfmt_misc approach instead.  */
	if (config.qemu && !is_host_elf(t_interp)) {
		status = push_args(false, &argv, 1, u_interp);
		if (status < 0)
			goto end;

		status = push_args(true, &argv, -1, config.qemu);
		if (status < 0)
			goto end;

		envp_has_changed = ldso_env_passthru(&envp, &argv, "-E", "-U");

		/* Errors are not fatal here.  */
		if (!argv_has_changed)
			push_args(false, &argv, 2, "-0", argv0);
		else
			push_args(false, &argv, 2, "-0", u_interp);

		argv_has_changed = true;

		/* Delay the translation of the newly instantiated
		 * runner until it accesses the program to execute,
		 * that way the runner can first access its own files
		 * outside of the rootfs. */
		tracee->trigger = strdup(u_interp);
		if (tracee->trigger == NULL) {
			status = -ENOMEM;
			goto end;
		}

		/* Launch the runner actually. */
		strcpy(t_interp, config.qemu[0]);
	}
	else {
		bool inhibit_rpath = false;

		/* Provide information to the host dynamic linker to
		 * find host libraries (remember the guest root
		 * file-system contains libraries for the guest
		 * architecture only).  Errors are not fatal here.  */
		if (config.qemu) {
			status = rebuild_host_ldso_paths(t_interp, &envp);
			if (status < 0)
				goto end;
			inhibit_rpath = (status > 0);
			envp_has_changed = true;
		}

		status = expand_interp(tracee, u_interp, t_interp, u_path /* dummy */, &argv, extract_elf_interp);
		if (status < 0)
			goto end;
		argv_has_changed = argv_has_changed || (status > 0);

		if (inhibit_rpath) {
			/* Tell the dynamic linker to ignore RPATHs specified
			 * in the *main* program.  To disable the RPATH
			 * mechanism globally, we have to list all objects
			 * here (NYI).  Errors are not fatal here.  */
			status = push_args(false, &argv, 2, "--inhibit-rpath", "''");
			argv_has_changed = argv_has_changed || (status > 0);
		}
	}

	VERBOSE(4, "execve: %s", t_interp);

	status = set_sysarg_path(tracee, t_interp, SYSARG_1);
	if (status < 0)
		goto end;

	/* Rebuild argv[] only if something has changed. */
	if (argv_has_changed) {
		size = set_args(tracee, argv, SYSARG_2);
		if (size < 0) {
			status = size;
			goto end;
		}
	}

	/* Rebuild envp[] only if something has changed. */
	if (envp_has_changed) {
		size = set_args(tracee, envp, SYSARG_3);
		if (size < 0) {
			status = size;
			goto end;
		}
	}

end:
	free_args(envp);
	free_args(argv);

	if (argv0 != NULL) {
		free(argv0);
		argv0 = NULL;
	}

	if (status < 0)
		return status;

	return size + status;
}
