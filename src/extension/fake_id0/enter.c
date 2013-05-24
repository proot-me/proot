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

syscall_number = peek_reg(tracee, ORIGINAL, SYSARG_NUM);
switch (syscall_number) {
case PR_chroot:
case PR_chmod:
case PR_chown:
case PR_fchmod:
case PR_fchown:
case PR_lchown:
case PR_chown32:
case PR_fchown32:
case PR_lchown32:
case PR_fchmodat:
case PR_fchownat:
case PR_fstatat64:
case PR_newfstatat:
case PR_stat64:
case PR_lstat64:
case PR_fstat64:
case PR_stat:
case PR_lstat:
case PR_fstat:
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
case PR_setresuid:
case PR_setresgid:
case PR_setresuid32:
case PR_setresgid32:
case PR_getresuid:
case PR_getresuid32:
case PR_getresgid:
case PR_getresgid32:
	/* Force the sysexit stage under seccomp.  */
	tracee->restart_how = PTRACE_SYSCALL;
default:
	return 0;
}
