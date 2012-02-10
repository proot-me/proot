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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 */

switch (tracee->sysnum) {
case PR_getcwd: {
	char path[PATH_MAX];
	size_t new_size;
	size_t size;
	word_t output;

	result = peek_ureg(tracee, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	output = peek_ureg(tracee, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	size = (size_t) peek_ureg(tracee, SYSARG_2);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	if (size > PATH_MAX) {
		status = -ENAMETOOLONG;
		break;
	}

	if (size == 0) {
		status = -EINVAL;
		break;
	}

	/* The kernel does put the terminating NULL byte for
	 * getcwd(2).  */
	status = get_tracee_string(tracee, path, output, size);
	if (status < 0)
		goto end;

	/* No '\0' were found, that shouldn't happen...  */
	if (status >= size) {
		status = -ERANGE;
		break;
	}

	status = detranslate_path(path, true, true);
	if (status < 0)
		break;

	/* The original path doesn't require any transformation, i.e
	 * it is a symetric binding.  */
	if (status == 0)
		goto end;

	new_size = status;
	if (new_size > size) {
		status = -ERANGE;
		break;
	}

	/* Overwrite the path.  */
	status = copy_to_tracee(tracee, output, path, new_size);
	if (status < 0)
		goto end;

	/* The value of "status" is used to update the returned value
	 * in translate_syscall_exit().  */
	status = new_size;
}
	break;

case PR_readlink:
case PR_readlinkat: {
	char pointee[PATH_MAX];
	char pointer[PATH_MAX];
	size_t old_size;
	size_t new_size;
	size_t max_size;
	word_t output;

	result = peek_ureg(tracee, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	/* Error reported by the kernel.  */
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	old_size = result;

	output = peek_ureg(tracee, tracee->sysnum == PR_readlink
				   ? SYSARG_2 : SYSARG_3);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	max_size = (size_t) peek_ureg(tracee, tracee->sysnum == PR_readlink
					      ? SYSARG_3 : SYSARG_4);
	if (errno != 0) {
		status = -errno;
		goto end;
	}

	if (max_size > PATH_MAX) {
		status = -ENAMETOOLONG;
		break;
	}

	if (max_size == 0) {
		status = -EINVAL;
		break;
	}

	/* The kernel does NOT put the terminating NULL byte for
	 * getcwd(2).  */
	status = copy_from_tracee(tracee, pointee, output, old_size);
	if (status < 0)
		goto end;
	pointee[old_size] = '\0';

	/* Not optimal but safe (path is fully translated).  */
	status = get_sysarg_path(tracee, pointer, tracee->sysnum == PR_readlink
						  ? SYSARG_1 : SYSARG_2);
	if (status < 0)
		break;

	status = detranslate_path(pointee, false, !belongs_to_guestfs(pointer));
	if (status < 0)
		break;

	/* The original path doesn't require any transformation, i.e
	 * it is a symetric binding.  */
	if (status == 0)
		goto end;

	new_size = (status - 1 < max_size ? status - 1 : max_size);

	/* Overwrite the path.  */
	status = copy_to_tracee(tracee, output, pointee, new_size);
	if (status < 0)
		goto end;

	/* The value of "status" is used to update the returned value
	 * in translate_syscall_exit().  */
	status = new_size;
}
	break;

case PR_uname:
	if (config.kernel_release) {
		struct utsname utsname;
		word_t release_addr;
		size_t release_size;

		result = peek_ureg(tracee, SYSARG_RESULT);
		if (errno != 0) {
			status = -errno;
			goto end;
		}
		if ((int) result < 0) {
			status = 0;
			goto end;
		}

		result = peek_ureg(tracee, SYSARG_1);
		if (errno != 0) {
			status = -errno;
			goto end;
		}

		release_addr = result + offsetof(struct utsname, release);
		release_size = sizeof(utsname.release);

		status = get_tracee_string(tracee, &(utsname.release), release_addr, release_size);
		if (status < 0)
			goto end;

		strncpy(utsname.release, config.kernel_release, release_size);
		utsname.release[release_size - 1] = '\0';

		status = copy_to_tracee(tracee, release_addr, &(utsname.release), strlen(utsname.release) + 1);
		if (status < 0)
			goto end;
	}
	status = 0;
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
