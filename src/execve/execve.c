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

#include <sys/stat.h> /* lstat(2), S_ISREG(), */
#include <unistd.h>   /* access(2), lstat(2), */
#include <string.h>   /* string(3), */
#include <assert.h>   /* assert(3), */
#include <errno.h>    /* E*, */
#include <talloc.h>   /* talloc_, */

#include "execve/execve.h"
#include "execve/interp.h"
#include "execve/elf.h"
#include "execve/ldso.h"
#include "tracee/array.h"
#include "tracee/tracee.h"
#include "syscall/syscall.h"
#include "notice.h"
#include "path/path.h"
#include "path/binding.h"
#include "extension/extension.h"

#include "compat.h"

/**
 * Translate @u_path into @t_path and check if this latter exists, is
 * executable and is a regular file.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
static int translate_n_check(Tracee *tracee, char t_path[PATH_MAX], const char *u_path)
{
	struct stat statl;
	int status;

	status = translate_path(tracee, t_path, AT_FDCWD, u_path, true);
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
static int expand_interp(Tracee *tracee, const char *u_path, char t_interp[PATH_MAX],
			char u_interp[PATH_MAX], Array *argv, extract_interp_t callback,
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

	VERBOSE(tracee, 3, "expand shebang: -> %s %s %s", u_interp, argument, u_path);

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
 * Check if the binary @host_path is PRoot itself.  In that case, @argv and
 * @envp are used to reconfigure the current @tracee.  This function returns
 * -errno if an error occured, 0 if the binary isn't PRoot, and 1 if @tracee was
 * reconfigured correctly.
 */
static int handle_sub_reconf(Tracee *tracee, Array *argv, const char *host_path)
{
	static char *self_exe = NULL;
	Tracee *dummy = NULL;
	char path[PATH_MAX];
	char **argv_pod;
	int status;
	size_t i;

	/* The path to PRoot itself is cached.  */
	if (self_exe == NULL) {
		status = readlink("/proc/self/exe", path, PATH_MAX);
		if (status < 0 || status >= PATH_MAX)
			return 0;
		path[status] = '\0';

		self_exe = strdup(path);
		if (self_exe == NULL)
			return -ENOMEM;
	}

	/* Check if the executed program is PRoot itself.  */
	if (strcmp(host_path, self_exe) != 0 || argv->length <= 1)
		return 0;

	/* Rebuild a POD argv[], as expected by parse_config().  */
	argv_pod = talloc_size(tracee->ctx, argv->length * sizeof(char **));
	if (argv_pod == NULL)
		return -ENOMEM;

	for (i = 0; i < argv->length; i++) {
		status = read_item_string(argv, i, &argv_pod[i]);
		if (status < 0)
			return status;
	}

	/* This dummy tracee holds the new configuration that will be copied
	 * back to the original tracee if everything is OK.  */
	dummy = talloc_zero(tracee->ctx, Tracee);
	if (dummy == NULL)
		return -ENOMEM;

	dummy->fs = talloc_zero(dummy, FileSystemNameSpace);
	if (dummy->fs == NULL)
		return -ENOMEM;

	dummy->ctx = talloc_new(dummy);
	if (dummy->ctx == NULL)
		return -ENOMEM;

	/* Inform parse_config() that paths are relative to the current tracee.
	 * For instance, "-w ./foo" will be translated to "-w
	 * ${tracee->cwd}/foo".  */
	dummy->reconf.tracee = tracee;
	dummy->reconf.paths = NULL;

	status = parse_config(dummy, argv->length - 1, argv_pod);
	if (status < 0)
		return -ECANCELED;
	bzero(&dummy->reconf, sizeof(dummy->reconf));

	/* How many arguments for the actual command?  */
	for (i = 0; dummy->cmdline[i] != NULL; i++)
		;

	/* Sanity checks.  */
	if (i < 1 || i >= argv->length) {
		notice(tracee, WARNING, INTERNAL, "wrong number of arguments (%zd)", i);
		return -ECANCELED;
	}

	/* Write the actual command back to the tracee's memory.  */
	status = resize_array(argv, argv->length - 1, i + 1 - argv->length);
	if (status < 0)
		return status;

	for (i = 0; dummy->cmdline[i] != NULL; i++)
		write_item_string(argv, i, dummy->cmdline[i]);
	write_item_string(argv, i, NULL);

	status = push_array(argv, SYSARG_2);
	if (status < 0)
		return status;

	status = set_sysarg_path(tracee, dummy->exe, SYSARG_1);
	if (status < 0)
		return status;

	/* Commit the new configuration.  */
	status = swap_config(tracee, dummy);
	if (status < 0)
		return status;

	/* Restart the execve() but with the actual command.  */
	status = translate_execve(tracee);
	if (status < 0) {
		/* Something went wrong, revert the new configuration.  Maybe
		 * this should be done in syscall/exit.c.  */
		(void) swap_config(tracee, dummy);
		return status;
	}

	inherit_extensions(tracee, dummy, true);

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
int translate_execve(Tracee *tracee)
{
	char u_path[PATH_MAX];
	char t_interp[PATH_MAX];
	char u_interp[PATH_MAX];

	Array *envp = NULL;
	Array *argv = NULL;
	char *argv0 = NULL;

	char **new_cmdline;
	char *new_exe;
	size_t i;

	bool ignore_elf_interpreter;
	bool inhibit_rpath = false;
	bool is_script;
	int status;

	status = get_sysarg_path(tracee, u_path, SYSARG_1);
	if (status < 0)
		return status;

	status = fetch_array(tracee, &argv, SYSARG_2, 0);
	if (status < 0)
		return status;

	if (tracee->qemu) {
		status = read_item_string(argv, 0, &argv0);
		if (status < 0)
			return status;

		/* Save the initial argv[0] since it will be replaced
		 * by tracee->qemu[0].  Errors are not fatal here.  */
		if (argv0 != NULL)
			argv0 = talloc_strdup(tracee->ctx, argv0);

		status = fetch_array(tracee, &envp, SYSARG_3, 0);
		if (status < 0)
			return status;

		/* Environment variables should be compared with the
		 * "name" part in the "name=value" string format.  */
		envp->compare_item = (compare_item_t)compare_item_env;
	}

	status = expand_interp(tracee, u_path, t_interp, u_interp, argv,
			       extract_script_interp, false);
	if (status < 0)
		/* The Linux kernel actually returns -EACCES when
		 * trying to execute a directory.  */
		return status == -EISDIR ? -EACCES : status;

	is_script = (status > 0);

	/* It's the rigth place to check if the binary is PRoot itself.  */
	status = handle_sub_reconf(tracee, argv, t_interp);
	if (status < 0)
		return status;
	if (status > 0)
		return 0;

	/* Remember the value for "/proc/self/exe".  Note that it points to a
	 * canonicalized guest path, hence detranslate_path().  We re-use u_path
	 * since it is not useful anymore.  */
	strcpy(u_path, t_interp);
	(void) detranslate_path(tracee, u_path, NULL);

	new_exe = talloc_strdup(tracee->ctx, u_path);
	if (new_exe == NULL)
		return -ENOMEM;

	/* Remember the value for "/proc/self/cmdline".  */
	new_cmdline = talloc_zero_array(tracee->ctx, char *, argv->length);
	if (new_cmdline == NULL)
		return -ENOMEM;

	for (i = 0; i < argv->length; i++) {
		char *ptr;
		status = read_item_string(argv, i, &ptr);
		if (status < 0)
			return status;
		/* It's safe to reference these strings since they are never
		 * overwritten, they are just replaced.  */
		new_cmdline[i] = talloc_reference(new_cmdline, ptr);
	}

	if (tracee->qemu != NULL) {
		/* Prepend the QEMU command to the initial argv[] if
		 * it's a "foreign" binary.  */
		if (!is_host_elf(tracee, t_interp)) {
			int i;

			status = resize_array(argv, 0, 3);
			if (status < 0)
				return status;

			/* For example, the second argument of:
			 *     execve("/bin/true", { "true", NULL }, ...)
			 * becomes:
			 *     { "/usr/bin/qemu", "-0", "true", "/bin/true"}  */
			status = write_items(argv, 0, 4, tracee->qemu[0], "-0",
					!is_script && argv0 != NULL ? argv0 : u_interp, u_interp);
			if (status < 0)
				return status;

			status = ldso_env_passthru(envp, argv, "-E", "-U");
			if (status < 0)
				return status;

			/* Compute the number of QEMU's arguments and
			 * add them to the modified argv[].  */
			for (i = 1; tracee->qemu[i] != NULL; i++)
				;
			status = resize_array(argv, 1, i - 1);
			if (status < 0)
				return status;

			for (i--; i > 0; i--) {
				status = write_item(argv, i, tracee->qemu[i]);
				if (status < 0)
					return status;
			}

			/* Launch the runner actually. */
			strcpy(t_interp, tracee->qemu[0]);
			status = join_paths(2, u_interp, HOST_ROOTFS, tracee->qemu[0]);
			if (status < 0)
				return status;
		}

		/* Provide information to the host dynamic linker to
		 * find host libraries (remember the guest root
		 * file-system contains libraries for the guest
		 * architecture only).  */
		status = rebuild_host_ldso_paths(tracee, t_interp, envp);
		if (status < 0)
			return status;
		inhibit_rpath = (status > 0);
	}

	/* Dont't use the ELF interpreter as a loader if the host one
	 * is compatible (currently the test is only "guest rootfs ==
	 * host rootfs") or if there's no need for RPATH inhibition in
	 * mixed-mode.  */
	ignore_elf_interpreter = (compare_paths(get_root(tracee), "/") == PATHS_ARE_EQUAL
				  || (tracee->qemu_pie_workaround && !inhibit_rpath));

	status = expand_interp(tracee, u_interp, t_interp, u_path /* dummy */,
			       argv, extract_elf_interp, ignore_elf_interpreter);
	if (status < 0)
		return status;

	if (status > 0 && inhibit_rpath) {
		/* Tell the dynamic linker to ignore RPATHs specified
		 * in the *main* program.  To disable the RPATH
		 * mechanism globally, we have to list all objects
		 * here (NYI).  Errors are not fatal here.  */
		status = resize_array(argv, 1, 2);
		if (status >= 0) {
			status = write_items(argv, 1, 2, "--inhibit-rpath", "''");
			if (status < 0)
				return status;
		}
	}

	VERBOSE(tracee, 4, "execve: %s", t_interp);

	status = set_sysarg_path(tracee, t_interp, SYSARG_1);
	if (status < 0)
		return status;

	status = push_array(argv, SYSARG_2);
	if (status < 0)
		return status;

	status = push_array(envp, SYSARG_3);
	if (status < 0)
		return status;

	/* So far so good, we can now safely update tracee->exe and
	 * tracee->cmdline.  Actually it would be safer in syscall/exit.c
	 * however I'm not able to write a test where execve(2) would fail at
	 * kernel level but not in PRoot.  Moreover this would require to store
	 * temporarily the original values for exe and cmdline, that is, before
	 * the insertion of the loader (ELF interpreter or QEMU).  */
	talloc_unlink(tracee, tracee->exe);
	tracee->exe = talloc_reference(tracee, new_exe);
	talloc_set_name_const(tracee->exe, "$exe");

	talloc_unlink(tracee, tracee->cmdline);
	tracee->cmdline = talloc_reference(tracee, new_cmdline);
	talloc_set_name_const(tracee->cmdline, "@cmdline");

	return 0;
}
