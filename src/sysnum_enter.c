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
 * Inspired by: QEMU user-mode.
 */

switch (child->sysnum) {
case __NR__llseek:
case __NR__newselect:
case __NR__sysctl:
case __NR_accept:
case __NR_accept4:
case __NR_add_key:
case __NR_adjtimex:
case __NR_afs_syscall:
case __NR_alarm:
case __NR_arch_prctl:
case __NR_bdflush:
case __NR_bind:
case __NR_break:
case __NR_brk:
case __NR_cacheflush:
case __NR_capget:
case __NR_capset:
case __NR_clock_getres:
case __NR_clock_gettime:
case __NR_clock_nanosleep:
case __NR_clock_settime:
case __NR_clone:
case __NR_close:
case __NR_connect:
case __NR_create_module:
case __NR_delete_module:
case __NR_dup:
case __NR_dup2:
case __NR_dup3:
case __NR_epoll_create:
case __NR_epoll_create1:
case __NR_epoll_ctl:
case __NR_epoll_ctl_old:
case __NR_epoll_pwait:
case __NR_epoll_wait:
case __NR_epoll_wait_old:
case __NR_eventfd:
case __NR_eventfd2:
case __NR_exit:
case __NR_exit_group:
case __NR_fadvise64:
case __NR_fadvise64_64:
case __NR_fallocate:
case __NR_fanotify_init:
case __NR_fanotify_mark:
case __NR_fchdir:
case __NR_fchmod:
case __NR_fchown:
case __NR_fchown32:
case __NR_fcntl:
case __NR_fcntl64:
case __NR_fdatasync:
case __NR_fgetxattr:
case __NR_flistxattr:
case __NR_flock:
case __NR_fork:
case __NR_fremovexattr:
case __NR_fsetxattr:
case __NR_fstat:
case __NR_fstat64:
case __NR_fstatfs:
case __NR_fstatfs64:
case __NR_fsync:
case __NR_ftime:
case __NR_ftruncate:
case __NR_ftruncate64:
case __NR_futex:
case __NR_get_kernel_syms:
case __NR_get_mempolicy:
case __NR_get_robust_list:
case __NR_get_thread_area:
case __NR_getcpu:
case __NR_getdents:
case __NR_getdents64:
case __NR_getegid:
case __NR_getegid32:
case __NR_geteuid:
case __NR_geteuid32:
case __NR_getgid:
case __NR_getgid32:
case __NR_getgroups:
case __NR_getgroups32:
case __NR_getitimer:
case __NR_getpeername:
case __NR_getpgid:
case __NR_getpgrp:
case __NR_getpid:
case __NR_getpmsg:
case __NR_getppid:
case __NR_getpriority:
case __NR_getresgid:
case __NR_getresgid32:
case __NR_getresuid:
case __NR_getresuid32:
case __NR_getrlimit:
case __NR_getrusage:
case __NR_getsid:
case __NR_getsockname:
case __NR_getsockopt:
case __NR_gettid:
case __NR_gettimeofday:
case __NR_getuid:
case __NR_getuid32:
case __NR_gtty:
case __NR_idle:
case __NR_init_module:
case __NR_inotify_init:
case __NR_inotify_init1:
case __NR_inotify_rm_watch:
case __NR_io_cancel:
case __NR_io_destroy:
case __NR_io_getevents:
case __NR_io_setup:
case __NR_io_submit:
case __NR_ioctl:
case __NR_ioperm:
case __NR_iopl:
case __NR_ioprio_get:
case __NR_ioprio_set:
case __NR_ipc:
case __NR_kexec_load:
case __NR_keyctl:
case __NR_kill:
case __NR_listen:
case __NR_lock:
case __NR_lookup_dcookie:
case __NR_lseek:
case __NR_madvise:
#if (__NR_madvise1 != __NR_madvise)
case __NR_madvise1: /* On i386 both values are equal. */
#endif
case __NR_mbind:
case __NR_migrate_pages:
case __NR_mincore:
case __NR_mlock:
case __NR_mlockall:
case __NR_mmap:
case __NR_mmap2:
case __NR_modify_ldt:
case __NR_move_pages:
case __NR_mprotect:
case __NR_mpx:
case __NR_mq_getsetattr:
case __NR_mq_notify:
case __NR_mq_open:
case __NR_mq_timedreceive:
case __NR_mq_timedsend:
case __NR_mq_unlink:
case __NR_mremap:
case __NR_msgctl:
case __NR_msgget:
case __NR_msgrcv:
case __NR_msgsnd:
case __NR_msync:
case __NR_munlock:
case __NR_munlockall:
case __NR_munmap:
case __NR_nanosleep:
case __NR_nfsservctl:
case __NR_nice:
case __NR_oldfstat:
case __NR_oldolduname:
case __NR_olduname:
case __NR_pause:
case __NR_pciconfig_iobase:
case __NR_pciconfig_read:
case __NR_pciconfig_write:
case __NR_perf_event_open:
case __NR_personality:
case __NR_pipe:
case __NR_pipe2:
case __NR_poll:
case __NR_ppoll:
case __NR_prctl:
case __NR_pread64:
case __NR_preadv:
case __NR_prlimit64:
case __NR_prof:
case __NR_profil:
case __NR_pselect6:
case __NR_putpmsg:
case __NR_pwrite64:
case __NR_pwritev:
case __NR_query_module:
case __NR_quotactl:
case __NR_read:
case __NR_readahead:
case __NR_readdir:
case __NR_readv:
case __NR_reboot:
case __NR_recv:
case __NR_recvfrom:
case __NR_recvmmsg:
case __NR_recvmsg:
case __NR_remap_file_pages:
case __NR_request_key:
case __NR_rt_sigaction:
case __NR_rt_sigpending:
case __NR_rt_sigprocmask:
case __NR_rt_sigqueueinfo:
case __NR_rt_sigreturn:
case __NR_rt_sigsuspend:
case __NR_rt_sigtimedwait:
case __NR_rt_tgsigqueueinfo:
case __NR_sched_get_priority_max:
case __NR_sched_get_priority_min:
case __NR_sched_getaffinity:
case __NR_sched_getparam:
case __NR_sched_getscheduler:
case __NR_sched_rr_get_interval:
case __NR_sched_setaffinity:
case __NR_sched_setparam:
case __NR_sched_setscheduler:
case __NR_sched_yield:
case __NR_security:
case __NR_select:
case __NR_semctl:
case __NR_semget:
case __NR_semop:
case __NR_semtimedop:
case __NR_send:
case __NR_sendfile:
case __NR_sendfile64:
case __NR_sendmsg:
case __NR_sendto:
case __NR_set_mempolicy:
case __NR_set_robust_list:
case __NR_set_thread_area:
case __NR_set_tid_address:
case __NR_setdomainname:
case __NR_setfsgid:
case __NR_setfsgid32:
case __NR_setfsuid:
case __NR_setfsuid32:
case __NR_setgid:
case __NR_setgid32:
case __NR_setgroups:
case __NR_setgroups32:
case __NR_sethostname:
case __NR_setitimer:
case __NR_setpgid:
case __NR_setpriority:
case __NR_setregid:
case __NR_setregid32:
case __NR_setresgid:
case __NR_setresgid32:
case __NR_setresuid:
case __NR_setresuid32:
case __NR_setreuid:
case __NR_setreuid32:
case __NR_setrlimit:
case __NR_setsid:
case __NR_setsockopt:
case __NR_settimeofday:
case __NR_setuid:
case __NR_setuid32:
case __NR_sgetmask:
case __NR_shmat:
case __NR_shmctl:
case __NR_shmdt:
case __NR_shmget:
case __NR_shutdown:
case __NR_sigaction:
case __NR_sigaltstack:
case __NR_signal:
case __NR_signalfd:
case __NR_signalfd4:
case __NR_sigpending:
case __NR_sigprocmask:
case __NR_sigreturn:
case __NR_sigsuspend:
case __NR_socket:
case __NR_socketcall:
case __NR_socketpair:
case __NR_splice:
case __NR_ssetmask:
case __NR_stime:
case __NR_streams1:
case __NR_streams2:
case __NR_stty:
case __NR_sync:
case __NR_sync_file_range:
/* case __NR_sync_file_range2: */
case __NR_sysfs:
case __NR_sysinfo:
case __NR_syslog:
case __NR_tee:
case __NR_tgkill:
case __NR_time:
case __NR_timer_create:
case __NR_timer_delete:
case __NR_timer_getoverrun:
case __NR_timer_gettime:
case __NR_timer_settime:
case __NR_timerfd_create:
case __NR_timerfd_gettime:
case __NR_timerfd_settime:
case __NR_times:
case __NR_tkill:
case __NR_tuxcall:
case __NR_ugetrlimit:
case __NR_ulimit:
case __NR_umask:
case __NR_uname:
case __NR_unshare:
case __NR_ustat:
case __NR_vfork:
case __NR_vhangup:
case __NR_vm86:
case __NR_vm86old:
case __NR_vmsplice:
case __NR_vserver:
case __NR_wait4:
case __NR_waitid:
case __NR_waitpid:
case __NR_write:
case __NR_writev:

#if defined(ARCH_ARM_EABI)
case __ARM_NR_breakpoint:
case __ARM_NR_cacheflush:
case __ARM_NR_set_tls:
case __ARM_NR_usr26:
case __ARM_NR_usr32:
case __NR_arm_fadvise64_64:
case __NR_arm_sync_file_range:
#endif /*  */

	/* Nothing to do. */
	status = 0;
	break;

case __NR_restart_syscall:
	/* I'm not able to know if I catched this syscall at the
	 * entry or the exit stage, and actually we just don't care™. */
	child->sysnum = (word_t)-1;
	status = 0;
	break;

case __NR_ptrace:
	if (!allow_ptrace)
		status = -EPERM;
	else
		status = 0;
	break;

case __NR_getcwd:
	child->output = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status = 0;
	break;

case __NR_execve:
	status = translate_execve(child);

	/* The stack is already saved/restored before/after an
	 * execve() for this architecture, look at this code:
	 * linux/arch/x86/kernel/entry_64.S:stub_execve */
#if defined(ARCH_X86_64)
	if (status > 0)
		status = 0;
#endif
	break;

case __NR_access:
case __NR_acct:
case __NR_chdir:
case __NR_chmod:
case __NR_chown:
case __NR_chown32:
case __NR_chroot:
case __NR_getxattr:
case __NR_listxattr:
case __NR_mkdir:
case __NR_mknod:
case __NR_oldstat:
case __NR_creat:
case __NR_removexattr:
case __NR_rmdir:
case __NR_setxattr:
case __NR_stat:
case __NR_stat64:
case __NR_statfs:
case __NR_statfs64:
case __NR_swapoff:
case __NR_swapon:
case __NR_truncate:
case __NR_truncate64:
case __NR_umount:
case __NR_umount2:
case __NR_uselib:
case __NR_utime:
case __NR_utimes:
	status = translate_sysarg(child, SYSARG_1, REGULAR);
	break;

case __NR_open:
	flags = peek_ureg(child, SYSARG_2);
	if (errno != 0) {
		status = -errno;
		break;
	}

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_sysarg(child, SYSARG_1, SYMLINK);
	else
		status = translate_sysarg(child, SYSARG_1, REGULAR);
	break;

case __NR_faccessat:
case __NR_fchmodat:
case __NR_fchownat:
case __NR_fstatat64:
case __NR_newfstatat:
case __NR_utimensat:
	dirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status = get_sysarg_path(child, path, SYSARG_2);
	if (status < 0)
		break;

	flags = (child->sysnum == __NR_fchownat)
		? peek_ureg(child, SYSARG_5)
		: peek_ureg(child, SYSARG_4);
	if (errno != 0) {
		status = -errno;
		break;
	}

	if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
		status = translate_path2(child, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(child, dirfd, path, SYSARG_2, REGULAR);
	break;

case __NR_futimesat:
case __NR_mkdirat:
case __NR_mknodat:
	dirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status = get_sysarg_path(child, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(child, dirfd, path, SYSARG_2, REGULAR);
	break;

case __NR_inotify_add_watch:
	flags = peek_ureg(child, SYSARG_3);
	if (errno != 0) {
		status = -errno;
		break;
	}

	if ((flags & IN_DONT_FOLLOW) != 0)
		status = translate_sysarg(child, SYSARG_2, SYMLINK);
	else
		status = translate_sysarg(child, SYSARG_2, REGULAR);
	break;

case __NR_lchown:
case __NR_lchown32:
case __NR_lgetxattr:
case __NR_llistxattr:
case __NR_lremovexattr:
case __NR_lsetxattr:
case __NR_lstat:
case __NR_lstat64:
case __NR_oldlstat:
case __NR_unlink:
case __NR_readlink:
	status = translate_sysarg(child, SYSARG_1, SYMLINK);
	if (child->sysnum == __NR_readlink) {
		child->output = peek_ureg(child, SYSARG_2);
		if (errno != 0) {
			status = -errno;
			break;
		}
	}
	break;

case __NR_link:
case __NR_pivot_root:
	status1 = translate_sysarg(child, SYSARG_1, REGULAR);
	status2 = translate_sysarg(child, SYSARG_2, REGULAR);
	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}
	status = status1 + status2;
	break;

case __NR_linkat:
	olddirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	newdirfd = peek_ureg(child, SYSARG_3);
	if (errno != 0) {
		status = -errno;
		break;
	}

	flags    = peek_ureg(child, SYSARG_5);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status1 = get_sysarg_path(child, oldpath, SYSARG_2);
	status2 = get_sysarg_path(child, newpath, SYSARG_4);
	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}

	if ((flags & AT_SYMLINK_FOLLOW) != 0)
		status1 = translate_path2(child, olddirfd, oldpath, SYSARG_2, REGULAR);
	else
		status1 = translate_path2(child, olddirfd, oldpath, SYSARG_2, SYMLINK);
	status2 = translate_path2(child, newdirfd, newpath, SYSARG_4, REGULAR);

	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}
	status = status1 + status2;
	break;

case __NR_mount:
	status = get_sysarg_path(child, path, SYSARG_1);
	if (status < 0)
		break;

	/* The following check covers only 90% of the cases. */
	if (path[0] == '/' || path[0] == '.') {
		status = translate_path2(child, AT_FDCWD, path, SYSARG_1, REGULAR);
		if (status < 0)
			break;
	}

	status = translate_sysarg(child, SYSARG_2, REGULAR);
	break;

case __NR_openat:
	dirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	flags = peek_ureg(child, SYSARG_3);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status = get_sysarg_path(child, path, SYSARG_2);
	if (status < 0)
		break;

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_path2(child, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(child, dirfd, path, SYSARG_2, REGULAR);
	break;

case __NR_unlinkat:
case __NR_readlinkat:
	dirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	if (child->sysnum == __NR_readlinkat) {
		child->output = peek_ureg(child, SYSARG_3);
		if (errno != 0) {
			status = -errno;
			break;
		}
	}

	status = get_sysarg_path(child, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(child, dirfd, path, SYSARG_2, SYMLINK);
	break;

case __NR_rename:
	status1 = translate_sysarg(child, SYSARG_1, SYMLINK);
	status2 = translate_sysarg(child, SYSARG_2, SYMLINK);

	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}
	status = status1 + status2;

	break;

case __NR_renameat:
	olddirfd = peek_ureg(child, SYSARG_1);
	if (errno != 0) {
		status = -errno;
		break;
	}

	newdirfd = peek_ureg(child, SYSARG_3);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status1 = get_sysarg_path(child, oldpath, SYSARG_2);
	status2 = get_sysarg_path(child, newpath, SYSARG_4);
	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}

	status1 = translate_path2(child, olddirfd, oldpath, SYSARG_2, SYMLINK);
	status2 = translate_path2(child, newdirfd, newpath, SYSARG_4, SYMLINK);
	if (status1 < 0) {
		status = status1;
		break;
	}
	if (status2 < 0) {
		status = status2;
		break;
	}

	status = status1 + status2;
	break;

case __NR_symlink:
	status = translate_sysarg(child, SYSARG_2, REGULAR);
	break;

case __NR_symlinkat:
	newdirfd = peek_ureg(child, SYSARG_2);
	if (errno != 0) {
		status = -errno;
		break;
	}

	status = get_sysarg_path(child, newpath, SYSARG_3);
	if (status < 0)
		break;

	status = translate_path2(child, newdirfd, newpath, SYSARG_3, REGULAR);
	break;

default:
	notice(WARNING, INTERNAL, "unknown syscall %lu", child->sysnum);
	if (!allow_unknown)
		status = -ENOSYS;
	else
		status = 0;
	break;
}
