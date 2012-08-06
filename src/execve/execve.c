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
#include "execve/interp.h"
#include "execve/elf.h"
#include "execve/ldso.h"
#include "tracee/array.h"
#include "syscall/syscall.h"
#include "notice.h"
#include "path/path.h"
#include "config.h"

#include "compat.h"

/**
 * Translate @u_path into @t_path and check if this latter exists, is
 * executable and is a regular file.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
static int translate_n_check(struct tracee *tracee, char t_path[PATH_MAX], const char *u_path)
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
	if (status < 0)
		return -EPERM;

	return 0;
}

/**
 * Substitute *@argv[0] with the interpreter (and its argument) of the
 * program pointed to by @u_path. The paths to the interpreter (or to
 * the program itself if it doesn't use an interpreter) are stored in
 * @t_interp and @u_interp (respectively translated and untranslated).
 */
static int expand_interp(struct tracee *tracee,
			 const char *u_path,
			 char t_interp[PATH_MAX],
			 char u_interp[PATH_MAX],
			 struct array *argv,
			 extract_interp_t callback,
			 bool ignore_interpreter)
{
	char argument[ARG_MAX];

	char dummy_path[PATH_MAX];
	char dummy_arg[ARG_MAX];

	int status;

	status = translate_n_check(tracee, t_interp, u_path);
	if (status < 0)
		return status;

	/* Skip the extraction of the interpreter on demand, in
	 * this case we execute the translation of u_path (t_interp)
	 * directly. */
	if (ignore_interpreter) {
		strcpy(u_interp, u_path);
		return 0;
	}

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
	 * ensures the interpreter is in the ELF format.  */
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

	VERBOSE(3, "expand shebang: -> %s %s %s", u_interp, argument, u_path);

	if (argument[0] != '\0') {
		status = resize_array(argv, 0, 2);
		if (status < 0)
			return status;

		status = write_items(argv, 0, 3, u_interp, argument, u_path);
		if (status < 0)
			return status;
	}
	else {
		status = resize_array(argv, 0, 1);
		if (status < 0)
			return status;

		status = write_items(argv, 0, 2, u_interp, u_path);
		if (status < 0)
			return status;
	}

	/* Remember at this point t_interp is the translation of
	 * u_interp. */
	return 1;
}

/**
 * Translate the arguments of the execve() syscall made by the @tracee
 * process.  This function return -errno if an error occured,
 * otherwise 0.
 *
 * The execve() syscall needs a very special treatment for script
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
int translate_execve(struct tracee *tracee)
{
	char u_path[PATH_MAX];
	char t_interp[PATH_MAX];
	char u_interp[PATH_MAX];

	struct array envp = { 0 };
	struct array argv = { 0 };
	char *argv0 = NULL;

	bool ignore_elf_interpreter;
	bool inhibit_rpath = false;
	bool is_script;
	int status;

	assert(tracee != NULL);

	status = get_sysarg_path(tracee, u_path, SYSARG_1);
	if (status < 0)
		return status;

	status = fetch_array(tracee, &argv, SYSARG_2, 0);
	if (status < 0)
		goto end;

	if (config.qemu) {
		status = read_item_string(&argv, 0, &argv0);
		if (status < 0)
			goto end;

		/* Save the initial argv[0] since it will be replaced
		 * by config.qemu[0].  */
		if (argv0 != NULL)
			argv0 = strdup(argv0);

		status = fetch_array(tracee, &envp, SYSARG_3, 0);
		if (status < 0)
			goto end;

		/* Environment variables should be compared with the
		 * "name" part in the "name=value" string format.  */
		envp.compare_item = (compare_item_t)compare_item_env;
	}

	status = expand_interp(tracee, u_path, t_interp, u_interp, &argv,
			       extract_script_interp, false);
	if (status < 0)
		goto end;
	is_script = (status > 0);

	/* "/proc/self/exe" points to a canonicalized path -- from the
	 * guest point-of-view --, hence detranslate_path(t_interp).
	 * We can re-use u_path since it is not useful anymore.  */
	strcpy(u_path, t_interp);
	(void) detranslate_path(tracee, u_path, NULL);
	if (tracee->exe != NULL)
		free(tracee->exe);
	tracee->exe = strdup(u_path);

	if (config.qemu) {
		/* Prepend the QEMU command to the initial argv[] if
		 * it's a "foreign" binary.  */
		if (!is_host_elf(t_interp)) {
			int i;

			status = resize_array(&argv, 0, 3);
			if (status < 0)
				goto end;

			status = write_items(&argv, 0, 3, config.qemu[0], "-0",
					!is_script && argv0 != NULL ? argv0 : u_interp);
			if (status < 0)
				goto end;

			status = ldso_env_passthru(&envp, &argv, "-E", "-U");
			if (status < 0)
				goto end;

			/* Compute the number of QEMU's arguments and
			 * add them to the modified argv[].  */
			for (i = 1; config.qemu[i] != NULL; i++)
				;
			status = resize_array(&argv, 1, i - 1);
			if (status < 0)
				goto end;

			for (i--; i > 0; i--) {
				status = write_item(&argv, i, config.qemu[i]);
				if (status < 0)
					goto end;
			}

			/* Launch the runner actually. */
			strcpy(t_interp, config.qemu[0]);
			status = join_paths(2, u_interp, config.host_rootfs, config.qemu[0]);
			if (status < 0)
				goto end;
		}

		/* Provide information to the host dynamic linker to
		 * find host libraries (remember the guest root
		 * file-system contains libraries for the guest
		 * architecture only).  */
		status = rebuild_host_ldso_paths(t_interp, &envp);
		if (status < 0)
			goto end;
		inhibit_rpath = (status > 0);
	}

	/* Dont't use the ELF interpreter as a loader if the host one
	 * is compatible (currently the test is only "guest rootfs ==
	 * host rootfs") or if there's no need for RPATH inhibition in
	 * mixed-mode.  */
	ignore_elf_interpreter = strcmp(config.guest_rootfs, "/") == 0
				 || (config.qemu != NULL && !inhibit_rpath);

	status = expand_interp(tracee, u_interp, t_interp, u_path /* dummy */,
			       &argv, extract_elf_interp, ignore_elf_interpreter);
	if (status < 0)
		goto end;

	if (status > 0 && inhibit_rpath) {
		/* Tell the dynamic linker to ignore RPATHs specified
		 * in the *main* program.  To disable the RPATH
		 * mechanism globally, we have to list all objects
		 * here (NYI).  Errors are not fatal here.  */
		status = resize_array(&argv, 1, 2);
		if (status >= 0) {
			status = write_items(&argv, 1, 2, "--inhibit-rpath", "''");
			if (status < 0)
				goto end;
		}
	}

	VERBOSE(4, "execve: %s", t_interp);

	status = set_sysarg_path(tracee, t_interp, SYSARG_1);
	if (status < 0)
		goto end;

	status = push_array(&argv, SYSARG_2);
	if (status < 0)
		goto end;

	status = push_array(&envp, SYSARG_3);
	if (status < 0)
		goto end;

	status = 0;

end:
	free_array(&argv);
	free_array(&envp);

	if (argv0 != NULL) {
		free(argv0);
		argv0 = NULL;
	}

	if (status < 0)
		return status;

	return 0;
}
