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

{
	word_t handled_syscalls[] = {
		/* Used by PRoot: */
		PR_accept,
		PR_accept4,
		PR_access,
		PR_acct,
		PR_bind,
		PR_chdir,
		PR_chmod,
		PR_chown,
		PR_chown32,
		PR_chroot,
		PR_connect,
		PR_creat,
		PR_execve,
		PR_faccessat,
		PR_fchdir,
		PR_fchmodat,
		PR_fchownat,
		PR_fstatat64,
		PR_futimesat,
		PR_getcwd,
		PR_getpeername,
		PR_getsockname,
		PR_getxattr,
		PR_inotify_add_watch,
		PR_lchown,
		PR_lchown32,
		PR_lgetxattr,
		PR_link,
		PR_linkat,
		PR_listxattr,
		PR_llistxattr,
		PR_lremovexattr,
		PR_lsetxattr,
		PR_lstat,
		PR_lstat64,
		PR_mkdir,
		PR_mkdirat,
		PR_mknod,
		PR_mknodat,
		PR_mount,
		PR_name_to_handle_at,
		PR_newfstatat,
		PR_oldlstat,
		PR_oldstat,
		PR_open,
		PR_openat,
		PR_pivot_root,
		PR_readlink,
		PR_readlinkat,
		PR_removexattr,
		PR_rename,
		PR_renameat,
		PR_rmdir,
		PR_rt_sigreturn,
		PR_setxattr,
		PR_sigreturn,
		PR_socketcall,
		PR_stat,
		PR_stat64,
		PR_statfs,
		PR_statfs64,
		PR_swapoff,
		PR_swapon,
		PR_symlink,
		PR_symlinkat,
		PR_truncate,
		PR_truncate64,
		PR_umount,
		PR_umount2,
		PR_uname,
		PR_unlink,
		PR_unlinkat,
		PR_uselib,
		PR_utime,
		PR_utimensat,
		PR_utimes,

		/* Used by fake_id0 but not by PRoot: */
		PR_fchmod,
		PR_fchown,
		PR_fchown32,
		PR_fstat,
		PR_fstat64,
		PR_getegid,
		PR_getegid32,
		PR_geteuid,
		PR_geteuid32,
		PR_getgid,
		PR_getgid32,
		PR_getresgid,
		PR_getresgid32,
		PR_getresuid,
		PR_getresuid32,
		PR_getuid,
		PR_getuid32,
		PR_setfsgid,
		PR_setfsgid32,
		PR_setfsuid,
		PR_setfsuid32,
		PR_setgid,
		PR_setgid32,
		PR_setresgid,
		PR_setresgid32,
		PR_setresuid,
		PR_setresuid32,
		PR_setuid,
		PR_setuid32,

		/* Used by kompat but not by PRoot: */
		PR__newselect,
		PR_dup2,
		PR_dup3,
		PR_epoll_create,
		PR_epoll_create1,
		PR_epoll_pwait,
		PR_epoll_wait,
		PR_eventfd,
		PR_eventfd2,
		PR_inotify_init,
		PR_inotify_init1,
		PR_pipe,
		PR_pipe2,
		PR_pselect6,
		PR_select,
		PR_signalfd,
		PR_signalfd4,
	};

	const size_t nb_handled_syscalls = sizeof(handled_syscalls) / sizeof(word_t);
	size_t nb_traced_syscalls;
	size_t i;

	/* Pre-compute the number of traced syscalls for this architecture.  */
	for (nb_traced_syscalls = 0, i = 0; i < nb_handled_syscalls; i++)
		if ((int) handled_syscalls[i] >= 0)
			nb_traced_syscalls++;

	/* Filter: if handled architecture */
	status = start_filter_arch_section(&program, PR_SECCOMP_ARCH, nb_traced_syscalls);
	if (status < 0)
		goto end;

	for (i = 0; i < nb_handled_syscalls; i++) {
		if ((int) handled_syscalls[i] < 0)
			continue;

		/* Filter: trace if handled syscall */
		status = add_filter_trace_syscall(&program, handled_syscalls[i],
						handled_syscalls[i] == PR_execve);
		if (status < 0)
			goto end;
	}

	/* Filter: allow unhandled syscalls for this architecture */
	status = end_filter_arch_section(&program, nb_traced_syscalls);
	if (status < 0)
		goto end;
}
