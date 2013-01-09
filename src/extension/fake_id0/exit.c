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

switch (peek_reg(tracee, ORIGINAL, SYSARG_NUM)) {
case PR_chroot: {
	char path[PATH_MAX];
	word_t result;
	word_t input;
	int status;

	/* Override only permission errors.  */
	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) result != -EPERM)
		return 0;

	input = peek_reg(tracee, MODIFIED, SYSARG_1);

	status = read_string(tracee, path, input, PATH_MAX);
	if (status < 0)
		return status;

	/* Only "new rootfs == current rootfs" is supported yet.  */
	status = compare_paths(get_root(tracee), path);
	if (status != PATHS_ARE_EQUAL)
		return 0;

	/* Force success.  */
	poke_reg(tracee, SYSARG_RESULT, 0);
	return 0;
}

case PR_chown:
case PR_fchown:
case PR_lchown:
case PR_chown32:
case PR_fchown32:
case PR_lchown32:
case PR_fchownat: {
	word_t result;

	/* Override only permission errors.  */
	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) result != -EPERM)
		return 0;

	/* Force success.  */
	poke_reg(tracee, SYSARG_RESULT, 0);
	return 0;
}

case PR_getuid:
case PR_getgid:
case PR_getegid:
case PR_geteuid:
case PR_getuid32:
case PR_getgid32:
case PR_geteuid32:
case PR_getegid32:
case PR_setuid:
case PR_setgid:
case PR_setfsuid:
case PR_setfsgid:
case PR_setuid32:
case PR_setgid32:
case PR_setfsuid32:
case PR_setfsgid32:
	/* Force success.  */
	poke_reg(tracee, SYSARG_RESULT, 0);
	return 0;

case PR_getresuid:
case PR_getresuid32:
case PR_getresgid:
case PR_getresgid32:
	/* TODO.  */

default:
	return 0;
}
