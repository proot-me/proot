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

/* This file is included by syscall.c, it can return two kinds of
 * status code:
 *
 * - break: update the syscall result register with "status"
 *
 * - goto end: "status < 0" means the tracee is dead, otherwise do
 *             nothing.
 */
switch (peek_reg(tracee, ORIGINAL, SYSARG_NUM)) {
case PR_getcwd: {
	size_t new_size;
	size_t size;
	word_t output;
	word_t result;

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	output = peek_reg(tracee, ORIGINAL, SYSARG_1);

	size = (size_t) peek_reg(tracee, ORIGINAL, SYSARG_2);
	if (size == 0) {
		status = -EINVAL;
		break;
	}

	new_size = strlen(tracee->fs->cwd) + 1;
	if (size < new_size) {
		status = -ERANGE;
		break;
	}

	/* Overwrite the path.  */
	status = write_data(tracee, output, tracee->fs->cwd, new_size);
	if (status < 0)
		goto end;

	/* The value of "status" is used to update the returned value
	 * in translate_syscall_exit().  */
	status = new_size;
	break;
}

case PR_fchdir:
case PR_chdir:
	/* These syscalls are fully emulated, see enter.c for details
	 * (like errors).  */
	status = 0;
	break;

case PR_readlink:
case PR_readlinkat: {
	char referee[PATH_MAX];
	char referer[PATH_MAX];
	size_t old_size;
	size_t new_size;
	size_t max_size;
	word_t input;
	word_t output;
	word_t result;

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	old_size = result;

	if (peek_reg(tracee, ORIGINAL, SYSARG_NUM) == PR_readlink) {
		output   = peek_reg(tracee, ORIGINAL, SYSARG_2);
		max_size = peek_reg(tracee, ORIGINAL, SYSARG_3);
		input    = peek_reg(tracee, MODIFIED, SYSARG_1);
	}
	else {
		output   = peek_reg(tracee, ORIGINAL,  SYSARG_3);
		max_size = peek_reg(tracee, ORIGINAL, SYSARG_4);
		input    = peek_reg(tracee, MODIFIED, SYSARG_2);
	}

	if (max_size > PATH_MAX)
		max_size = PATH_MAX;

	if (max_size == 0) {
		status = -EINVAL;
		break;
	}

	/* The kernel does NOT put the terminating NULL byte for
	 * getcwd(2).  */
	status = read_data(tracee, referee, output, old_size);
	if (status < 0)
		goto end;
	referee[old_size] = '\0';

	/* Not optimal but safe (path is fully translated).  */
	status = read_string(tracee, referer, input, PATH_MAX);
	if (status < 0)
		goto end;

	if (status >= PATH_MAX) {
		status = -ENAMETOOLONG;
		break;
	}

	status = detranslate_path(tracee, referee, referer);
	if (status < 0)
		break;

	/* The original path doesn't require any transformation, i.e
	 * it is a symetric binding.  */
	if (status == 0)
		goto end;

	new_size = (status - 1 < max_size ? status - 1 : max_size);

	/* Overwrite the path.  */
	status = write_data(tracee, output, referee, new_size);
	if (status < 0)
		goto end;

	/* The value of "status" is used to update the returned value
	 * in translate_syscall_exit().  */
	status = new_size;
	break;
}

#if defined(ARCH_X86_64)
case PR_uname: {
	struct utsname utsname;
	word_t address;
	word_t result;
	size_t size;

	if (get_abi(tracee) != ABI_2) {
		/* Nothing to do.  */
		status = 0;
		goto end;
	}

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	address = peek_reg(tracee, ORIGINAL, SYSARG_1);

	status = read_data(tracee, &utsname, address, sizeof(utsname));
	if (status < 0)
		goto end;

	/* Some 32-bit programs like package managers can be
	 * confused when the kernel reports "x86_64".  */
	size = sizeof(utsname.machine);
	strncpy(utsname.machine, "i686", size);
	utsname.machine[size - 1] = '\0';

	status = write_data(tracee, address, &utsname, sizeof(utsname));
	if (status < 0)
		goto end;

	status = 0;
	break;
}
#endif

case PR_execve:
	if ((int) peek_reg(tracee, CURRENT, SYSARG_RESULT) >= 0) {
case PR_rt_sigreturn:
case PR_sigreturn:
		restore_original_sp = false;
	}
	status = 0;
	goto end;

case PR_fork:
case PR_clone: {
	/* Note: vfork can't be handled here since the parent don't
	 * return until the child does a call to execve(2) and the
	 * child would be stopped by PRoot until the parent returns
	 * (deak-lock).  Instead it is handled asynchronously in
	 * event.c.  */

	Tracee *child;
	word_t result;
	word_t flags;

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	child = get_tracee(tracee, result, true);
	if (child == NULL) {
		status = -ENOMEM;
		break;
	}

	if (peek_reg(tracee, ORIGINAL, SYSARG_NUM) == PR_clone)
		flags = peek_reg(tracee, ORIGINAL, SYSARG_1);
	else
		flags = 0;

	status = inherit_config(child, tracee, (flags & CLONE_FS) != 0);
	if (status < 0)
		break;

	status = 0;
	goto end;
}

default:
	status = 0;
	goto end;
}
