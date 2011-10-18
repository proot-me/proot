/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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

/* Keep in sync' with detranslate_addr(). */
switch (tracee->sysnum) {
case __NR_getcwd:
	result = peek_ureg(tracee, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	status = detranslate_addr(tracee, tracee->output, result, GETCWD);
	break;

case __NR_readlink:
case __NR_readlinkat:
	result = peek_ureg(tracee, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	/* Avoid the detranslation of partial result. */
	status = (int) peek_ureg(tracee, tracee->sysnum == __NR_readlink ? SYSARG_3 : SYSARG_4);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result == status) {
		status = 0;
		goto end;
	}

	assert((int) result <= status);

	status = detranslate_addr(tracee, tracee->output, result, READLINK);
	break;

case __NR_uname:
	if (kernel_release != NULL) {
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

		strncpy(utsname.release, kernel_release, release_size);
		utsname.release[release_size - 1] = '\0';

		status = copy_to_tracee(tracee, release_addr, &(utsname.release), strlen(utsname.release) + 1);
		if (status < 0)
			goto end;
	}
	status = 0;
	break;

case __NR_getuid:
case __NR_getgid:
case __NR_getegid:
case __NR_geteuid:
case __NR_getuid32:
case __NR_getgid32:
case __NR_geteuid32:
case __NR_getegid32:
	status = 0;
	if (fake_id0)
		break;
	goto end;

case __NR_getresuid:
case __NR_getresuid32:
case __NR_getresgid:
case __NR_getresgid32:
	/* TODO.  */

default:
	status = 0;
	goto end;
}
