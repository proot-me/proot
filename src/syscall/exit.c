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

/* This file is included by syscall.c, it can return two kinds of
 * status code:
 *
 * - break: update the syscall result register with "status"
 *
 * - goto end: "status < 0" means the tracee is dead, otherwise do
 *             nothing.
 */
switch (tracee->sysnum) {
case PR_getcwd: {
	char path[PATH_MAX];
	size_t new_size;
	size_t size;
	word_t output;
	word_t result;

	result = peek_reg(tracee, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	output = peek_reg(tracee, SYSARG_1);

	size = (size_t) peek_reg(tracee, SYSARG_2);

	if (size > PATH_MAX)
		size = PATH_MAX;

	if (size == 0) {
		status = -EINVAL;
		break;
	}

	/* The kernel does put the terminating NULL byte for
	 * getcwd(2).  */
	status = read_string(tracee, path, output, size);
	if (status < 0)
		goto end;

	if (status >= size) {
		status = (size == PATH_MAX ? -ENAMETOOLONG : -ERANGE);
		break;
	}

	status = detranslate_path(tracee, path, NULL);
	if (status < 0)
		break;

	/* The original path doesn't require any transformation, i.e
	 * it is a symetric binding.  */
	if (status == 0)
		goto end;

	new_size = status;
	if (new_size > size) {
		status = (size == PATH_MAX ? -ENAMETOOLONG : -ERANGE);
		break;
	}

	/* Overwrite the path.  */
	status = write_data(tracee, output, path, new_size);
	if (status < 0)
		goto end;

	/* The value of "status" is used to update the returned value
	 * in translate_syscall_exit().  */
	status = new_size;
}
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

	result = peek_reg(tracee, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	old_size = result;

	output = peek_reg(tracee, tracee->sysnum == PR_readlink
				   ? SYSARG_2 : SYSARG_3);

	max_size = (size_t) peek_reg(tracee, tracee->sysnum == PR_readlink
					      ? SYSARG_3 : SYSARG_4);

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

	input = peek_reg(tracee, tracee->sysnum == PR_readlink
			        ? SYSARG_1 : SYSARG_2);

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
}
	break;

case PR_uname: {
	struct utsname utsname;
	word_t address;
	word_t result;
	size_t size;

	bool change_uname;

	change_uname = config.kernel_release != NULL
		       || get_abi(tracee) != ABI_DEFAULT;

	if (!change_uname) {
		/* Nothing to do.  */
		status = 0;
		goto end;
	}

	result = peek_reg(tracee, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	address = peek_reg(tracee, SYSARG_1);

	status = read_data(tracee, &utsname, address, sizeof(utsname));
	if (status < 0)
		goto end;

	/*
	 * Note: on x86_64, we can handle the two modes (32/64) with
	 * the same code since struct utsname as always the same
	 * layout.
	 */

	if (config.kernel_release) {
		size = sizeof(utsname.release);

		strncpy(utsname.release, config.kernel_release, size);
		utsname.release[size - 1] = '\0';
	}

#if defined(ARCH_X86_64)
	switch (get_abi(tracee)) {
	case ABI_DEFAULT:
		/* Nothing to do.  */
		break;

	case ABI_2:
		size = sizeof(utsname.machine);

		/* Some 32-bit programs like package managers can be
		 * confused when the kernel reports "x86_64".  */
		strncpy(utsname.machine, "i686", size);
		utsname.machine[size - 1] = '\0';
		break;

	default:
		assert(0);
	}
#else
	assert(get_abi(tracee) == ABI_DEFAULT);
#endif

	status = write_data(tracee, address, &utsname, sizeof(utsname));
	if (status < 0)
		goto end;

	status = 0;
}
	break;

case PR_chroot: {
	char path[PATH_MAX];
	word_t result;
	word_t input;

	if (!config.fake_id0) {
		status = 0;
		goto end;
	}

	result = peek_reg(tracee, SYSARG_RESULT);

	/* Override only permission errors.  */
	if ((int) result != -EPERM) {
		status = 0;
		goto end;
	}

	input = peek_reg(tracee, SYSARG_1);

	status = read_string(tracee, path, input, PATH_MAX);
	if (status < 0)
		goto end;

	/* Succeed only if the new rootfs == current rootfs.  */
	status = compare_paths2(root, root_length, path, strlen(path));
	if (status != PATHS_ARE_EQUAL) {
		status = 0;
		goto end;
	}

	status = 0;
}
	break;

case PR_chown:
case PR_fchown:
case PR_lchown:
case PR_chown32:
case PR_fchown32:
case PR_lchown32:
case PR_fchownat: {
	word_t result;

	if (!config.fake_id0) {
		status = 0;
		goto end;
	}

	result = peek_reg(tracee, SYSARG_RESULT);

	/* Override only permission errors.  */
	if ((int) result != -EPERM) {
		status = 0;
		goto end;
	}

	status = 0;
}
	break;

case PR_getuid:
case PR_getgid:
case PR_getegid:
case PR_geteuid:
case PR_getuid32:
case PR_getgid32:
case PR_geteuid32:
case PR_getegid32:
	status = 0;
	if (config.fake_id0)
		break;
	goto end;

case PR_getresuid:
case PR_getresuid32:
case PR_getresgid:
case PR_getresgid32:
	/* TODO.  */

default:
	status = 0;
	goto end;
}
