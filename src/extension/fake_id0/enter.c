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

/* Note: syscalls like "openat" can be replaced by "open" since PRoot
 * has canonicalized "fd + path" into "path".  */

Config *config = talloc_get_type_abort(extension->config, Config);
switch (peek_reg(tracee, CURRENT, SYSARG_NUM)) {
case PR_execve:
case PR_fchdir:
case PR_chdir:
case PR_access:
case PR_acct:
case PR_chmod:
case PR_chown:
case PR_chown32:
case PR_chroot:
case PR_getxattr:
case PR_listxattr:
case PR_mknod:
case PR_oldstat:
case PR_creat:
case PR_removexattr:
case PR_setxattr:
case PR_stat:
case PR_stat64:
case PR_statfs:
case PR_statfs64:
case PR_swapoff:
case PR_swapon:
case PR_truncate:
case PR_truncate64:
case PR_umount:
case PR_umount2:
case PR_uselib:
case PR_utime:
case PR_utimes:
case PR_open:
case PR_faccessat:
case PR_fchmodat:
case PR_fchownat:
case PR_fstatat64:
case PR_newfstatat:
case PR_utimensat:
case PR_name_to_handle_at:
case PR_futimesat:
case PR_mknodat:
case PR_inotify_add_watch:
case PR_lchown:
case PR_lchown32:
case PR_lgetxattr:
case PR_llistxattr:
case PR_lremovexattr:
case PR_lsetxattr:
case PR_lstat:
case PR_lstat64:
case PR_oldlstat:
case PR_unlink:
case PR_readlink:
case PR_rmdir:
case PR_mkdir:
case PR_pivot_root:
case PR_linkat:
case PR_mount:
case PR_openat:
case PR_unlinkat:
case PR_readlinkat:
case PR_mkdirat:
case PR_link:
case PR_rename:
case PR_renameat:
case PR_symlink:
case PR_symlinkat: {
	config->interesting_syscall = 1;
	return 0;
}

default:
	config->interesting_syscall = 0;
	return 0;
}
