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

switch (peek_reg(tracee, CURRENT, SYSARG_NUM)) {
case PR__llseek:
case PR__newselect:
case PR__sysctl:
case PR_accept:
case PR_accept4:
case PR_add_key:
case PR_adjtimex:
case PR_afs_syscall:
case PR_alarm:
case PR_arch_prctl:
case PR_bdflush:
case PR_bind:
case PR_break:
case PR_brk:
case PR_cacheflush:
case PR_capget:
case PR_capset:
case PR_clock_adjtime:
case PR_clock_getres:
case PR_clock_gettime:
case PR_clock_nanosleep:
case PR_clock_settime:
case PR_clone:
case PR_close:
case PR_connect:
case PR_create_module:
case PR_delete_module:
case PR_dup:
case PR_dup2:
case PR_dup3:
case PR_epoll_create:
case PR_epoll_create1:
case PR_epoll_ctl:
case PR_epoll_ctl_old:
case PR_epoll_pwait:
case PR_epoll_wait:
case PR_epoll_wait_old:
case PR_eventfd:
case PR_eventfd2:
case PR_exit:
case PR_exit_group:
case PR_fadvise64:
case PR_fadvise64_64:
case PR_fallocate:
case PR_fanotify_init:
case PR_fanotify_mark:
case PR_fchmod:
case PR_fchown:
case PR_fchown32:
case PR_fcntl:
case PR_fcntl64:
case PR_fdatasync:
case PR_fgetxattr:
case PR_flistxattr:
case PR_flock:
case PR_fork:
case PR_fremovexattr:
case PR_fsetxattr:
case PR_fstat:
case PR_fstat64:
case PR_fstatfs:
case PR_fstatfs64:
case PR_fsync:
case PR_ftime:
case PR_ftruncate:
case PR_ftruncate64:
case PR_futex:
case PR_get_kernel_syms:
case PR_get_mempolicy:
case PR_get_robust_list:
case PR_get_thread_area:
case PR_getcpu:
case PR_getcwd:
case PR_getdents:
case PR_getdents64:
case PR_getegid:
case PR_getegid32:
case PR_geteuid:
case PR_geteuid32:
case PR_getgid:
case PR_getgid32:
case PR_getgroups:
case PR_getgroups32:
case PR_getitimer:
case PR_getpeername:
case PR_getpgid:
case PR_getpgrp:
case PR_getpid:
case PR_getpmsg:
case PR_getppid:
case PR_getpriority:
case PR_getresgid:
case PR_getresgid32:
case PR_getresuid:
case PR_getresuid32:
case PR_getrlimit:
case PR_getrusage:
case PR_getsid:
case PR_getsockname:
case PR_getsockopt:
case PR_gettid:
case PR_gettimeofday:
case PR_getuid:
case PR_getuid32:
case PR_gtty:
case PR_idle:
case PR_init_module:
case PR_inotify_init:
case PR_inotify_init1:
case PR_inotify_rm_watch:
case PR_io_cancel:
case PR_io_destroy:
case PR_io_getevents:
case PR_io_setup:
case PR_io_submit:
case PR_ioctl:
case PR_ioperm:
case PR_iopl:
case PR_ioprio_get:
case PR_ioprio_set:
case PR_ipc:
case PR_kexec_load:
case PR_keyctl:
case PR_kill:
case PR_listen:
case PR_lock:
case PR_lookup_dcookie:
case PR_lseek:
case PR_madvise:
case PR_mbind:
case PR_migrate_pages:
case PR_mincore:
case PR_mlock:
case PR_mlockall:
case PR_mmap:
case PR_mmap2:
case PR_modify_ldt:
case PR_move_pages:
case PR_mprotect:
case PR_mpx:
case PR_mq_getsetattr:
case PR_mq_notify:
case PR_mq_open:
case PR_mq_timedreceive:
case PR_mq_timedsend:
case PR_mq_unlink:
case PR_mremap:
case PR_msgctl:
case PR_msgget:
case PR_msgrcv:
case PR_msgsnd:
case PR_msync:
case PR_munlock:
case PR_munlockall:
case PR_munmap:
case PR_nanosleep:
case PR_nfsservctl:
case PR_nice:
case PR_oldfstat:
case PR_oldolduname:
case PR_olduname:
case PR_open_by_handle_at:
case PR_pause:
case PR_pciconfig_iobase:
case PR_pciconfig_read:
case PR_pciconfig_write:
case PR_perf_event_open:
case PR_personality:
case PR_pipe:
case PR_pipe2:
case PR_poll:
case PR_ppoll:
case PR_prctl:
case PR_pread64:
case PR_preadv:
case PR_prlimit64:
case PR_process_vm_readv:
case PR_process_vm_writev:
case PR_prof:
case PR_profil:
case PR_pselect6:
case PR_ptrace:
case PR_putpmsg:
case PR_pwrite64:
case PR_pwritev:
case PR_query_module:
case PR_quotactl:
case PR_read:
case PR_readahead:
case PR_readdir:
case PR_readv:
case PR_reboot:
case PR_recv:
case PR_recvfrom:
case PR_recvmmsg:
case PR_recvmsg:
case PR_remap_file_pages:
case PR_request_key:
case PR_restart_syscall:
case PR_rt_sigaction:
case PR_rt_sigpending:
case PR_rt_sigprocmask:
case PR_rt_sigqueueinfo:
case PR_rt_sigreturn:
case PR_rt_sigsuspend:
case PR_rt_sigtimedwait:
case PR_rt_tgsigqueueinfo:
case PR_sched_get_priority_max:
case PR_sched_get_priority_min:
case PR_sched_getaffinity:
case PR_sched_getparam:
case PR_sched_getscheduler:
case PR_sched_rr_get_interval:
case PR_sched_setaffinity:
case PR_sched_setparam:
case PR_sched_setscheduler:
case PR_sched_yield:
case PR_security:
case PR_select:
case PR_semctl:
case PR_semget:
case PR_semop:
case PR_semtimedop:
case PR_send:
case PR_sendfile:
case PR_sendfile64:
case PR_sendmmsg:
case PR_sendmsg:
case PR_sendto:
case PR_set_mempolicy:
case PR_set_robust_list:
case PR_set_thread_area:
case PR_set_tid_address:
case PR_setdomainname:
case PR_setfsgid:
case PR_setfsgid32:
case PR_setfsuid:
case PR_setfsuid32:
case PR_setgid:
case PR_setgid32:
case PR_setgroups:
case PR_setgroups32:
case PR_sethostname:
case PR_setitimer:
case PR_setns:
case PR_setpgid:
case PR_setpriority:
case PR_setregid:
case PR_setregid32:
case PR_setresgid:
case PR_setresgid32:
case PR_setresuid:
case PR_setresuid32:
case PR_setreuid:
case PR_setreuid32:
case PR_setrlimit:
case PR_setsid:
case PR_setsockopt:
case PR_settimeofday:
case PR_setuid:
case PR_setuid32:
case PR_sgetmask:
case PR_shmat:
case PR_shmctl:
case PR_shmdt:
case PR_shmget:
case PR_shutdown:
case PR_sigaction:
case PR_sigaltstack:
case PR_signal:
case PR_signalfd:
case PR_signalfd4:
case PR_sigpending:
case PR_sigprocmask:
case PR_sigreturn:
case PR_sigsuspend:
case PR_socket:
case PR_socketcall:
case PR_socketpair:
case PR_splice:
case PR_ssetmask:
case PR_stime:
case PR_stty:
case PR_sync:
case PR_sync_file_range:
case PR_sync_file_range2:
case PR_syncfs:
case PR_sysfs:
case PR_sysinfo:
case PR_syslog:
case PR_tee:
case PR_tgkill:
case PR_time:
case PR_timer_create:
case PR_timer_delete:
case PR_timer_getoverrun:
case PR_timer_gettime:
case PR_timer_settime:
case PR_timerfd_create:
case PR_timerfd_gettime:
case PR_timerfd_settime:
case PR_times:
case PR_tkill:
case PR_tuxcall:
case PR_ugetrlimit:
case PR_ulimit:
case PR_umask:
case PR_uname:
case PR_unshare:
case PR_ustat:
case PR_vfork:
case PR_vhangup:
case PR_vm86:
case PR_vm86old:
case PR_vmsplice:
case PR_vserver:
case PR_wait4:
case PR_waitid:
case PR_waitpid:
case PR_write:
case PR_writev:

#if defined(ARCH_ARM_EABI)
case PR_ARM_breakpoint:
case PR_ARM_cacheflush:
case PR_ARM_set_tls:
case PR_ARM_usr26:
case PR_ARM_usr32:
case PR_arm_fadvise64_64:
case PR_arm_sync_file_range:
#endif

	/* Nothing to do. */
	status = 0;
	break;

case PR_execve:
	status = translate_execve(tracee);
	break;

case PR_fchdir:
case PR_chdir: {
	char *tmp;

	if (peek_reg(tracee, CURRENT, SYSARG_NUM) == PR_chdir) {
		status = get_sysarg_path(tracee, path, SYSARG_1);
		if (status < 0)
			break;

		/* The ending "." ensures canonicalize() will report
		 * an error if path does not exist or if it is not a
		 * directory.  */
		if (path[0] != '/')
			status = join_paths(3, newpath, tracee->fs->cwd, path, ".");
		else
			status = join_paths(2, newpath, path, ".");
		if (status < 0)
			break;

		/* Initiale state for canonicalization.  */
		strcpy(path, "/");

		status = canonicalize(tracee, newpath, true, path, 0);
		if (status < 0)
			break;
	}
	else {
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

		/* Sadly this method doesn't detranslate statefully,
		 * this means that there's an ambiguity when several
		 * bindings are from the same host path:
		 *
		 *    $ proot -m /tmp:/a -m /tmp:/b fchdir_getcwd /a
		 *    /b
		 *
		 *    $ proot -m /tmp:/b -m /tmp:/a fchdir_getcwd /a
		 *    /a
		 *
		 * A solution would be to follow each file descriptor
		 * just like it is done for cwd.
		 */

		status = translate_path(tracee, path, dirfd, ".", true);
		if (status < 0)
			break;

		status = detranslate_path(tracee, path, NULL);
		if (status < 0)
			break;
	}

	/* Remove the trailing "/" or "/.".  */
	chop_finality(path);

	tmp = talloc_strdup(tracee->fs, path);
	if (tmp == NULL) {
		status = -ENOMEM;
		break;
	}
	TALLOC_FREE(tracee->fs->cwd);

	tracee->fs->cwd = tmp;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");

	poke_reg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
	status = 0;
	break;
}

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
	status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	break;

case PR_open:
	flags = peek_reg(tracee, CURRENT, SYSARG_2);

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	else
		status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	break;

case PR_faccessat:
case PR_fchmodat:
case PR_fchownat:
case PR_fstatat64:
case PR_newfstatat:
case PR_utimensat:
case PR_name_to_handle_at:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	flags = (   peek_reg(tracee, CURRENT, SYSARG_NUM) == PR_fchownat
		 || peek_reg(tracee, CURRENT, SYSARG_NUM) == PR_name_to_handle_at)
		? peek_reg(tracee, CURRENT, SYSARG_5)
		: peek_reg(tracee, CURRENT, SYSARG_4);

	if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
		status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_futimesat:
case PR_mknodat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_inotify_add_watch:
	flags = peek_reg(tracee, CURRENT, SYSARG_3);

	if ((flags & IN_DONT_FOLLOW) != 0)
		status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	else
		status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

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
	status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	break;

case PR_pivot_root:
	status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	if (status < 0)
		break;

	status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

case PR_linkat:
	olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);
	flags    = peek_reg(tracee, CURRENT, SYSARG_5);

	status = get_sysarg_path(tracee, oldpath, SYSARG_2);
	if (status < 0)
		break;

	status = get_sysarg_path(tracee, newpath, SYSARG_4);
	if (status < 0)
		break;

	if ((flags & AT_SYMLINK_FOLLOW) != 0)
		status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, REGULAR);
	else
		status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
	break;

case PR_mount:
	status = get_sysarg_path(tracee, path, SYSARG_1);
	if (status < 0)
		break;

	/* The following check covers only 90% of the cases. */
	if (path[0] == '/' || path[0] == '.') {
		status = translate_path2(tracee, AT_FDCWD, path, SYSARG_1, REGULAR);
		if (status < 0)
			break;
	}

	status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

case PR_openat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	flags = peek_reg(tracee, CURRENT, SYSARG_3);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_unlinkat:
case PR_readlinkat:
case PR_mkdirat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	break;

case PR_link:
case PR_rename:
	status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	if (status < 0)
		break;

	status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	break;

case PR_renameat:
	olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);

	status = get_sysarg_path(tracee, oldpath, SYSARG_2);
	if (status < 0)
		break;

	status = get_sysarg_path(tracee, newpath, SYSARG_4);
	if (status < 0)
		break;

	status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
	break;

case PR_symlink:
	status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	break;

case PR_symlinkat:
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_2);

	status = get_sysarg_path(tracee, newpath, SYSARG_3);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_3, SYMLINK);
	break;

default:
	notice(tracee, WARNING, INTERNAL, "unknown syscall %ld",
		peek_reg(tracee, CURRENT, SYSARG_NUM));
	status = 0;
	break;
}
