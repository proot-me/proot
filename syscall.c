/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010 STMicroelectronics
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
 * Inspired by: strace and QEMU user-mode.
 */

#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <sys/types.h>  /* pid_t, */
#include <stdlib.h>     /* NULL, */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <errno.h>      /* errno, */
#include <stdio.h>      /* perror(3), fprintf(3), */

#include "syscall.h" /* enum arg_number, ARG_*, */
#include "arch.h"    /* REG_ARG_* */

/* Specify the offset in the USER area of each register used for
 * syscall argument passing. */
#define USER_REGS_OFFSET(reg_name) (offsetof(struct user, regs) + offsetof(struct user_regs_struct, reg_name))
size_t arg_offset[] = {
	USER_REGS_OFFSET(REG_ARG_SYSNUM),
	USER_REGS_OFFSET(REG_ARG_1),
	USER_REGS_OFFSET(REG_ARG_2),
	USER_REGS_OFFSET(REG_ARG_3),
	USER_REGS_OFFSET(REG_ARG_4),
	USER_REGS_OFFSET(REG_ARG_5),
	USER_REGS_OFFSET(REG_ARG_6),
	USER_REGS_OFFSET(REG_ARG_RESULT),
};

/**
 * Return the @arg_number-th argument of the current syscall in the
 * @pid process.
 */
unsigned long get_syscall_arg(pid_t pid, enum arg_number arg_number)
{
	unsigned long result;

	if (arg_number < ARG_FIRST || arg_number > ARG_LAST) {
		fprintf(stderr, "proot -- set_syscall_arg(%d) not supported\n", arg_number);
		exit(EXIT_FAILURE);
	}

	result = ptrace(PTRACE_PEEKUSER, pid, arg_offset[arg_number], NULL);
	if (errno != 0) {
		perror("proot -- ptrace(PEEKUSER)");
		exit(EXIT_FAILURE);
	}

	return result;
}

/**
 * Set the @arg_number-th argument of the current syscall in the @pid
 * process to @value.
 */
void set_syscall_arg(pid_t pid, enum arg_number arg_number, unsigned long value)
{
	unsigned long status = 0;

	if (arg_number < ARG_FIRST || arg_number > ARG_LAST) {
		fprintf(stderr, "proot -- set_syscall_arg(%d) not supported\n", arg_number);
		exit(EXIT_FAILURE);
	}

	status = ptrace(PTRACE_POKEUSER, pid, arg_offset[arg_number], value);
	if (status < 0) {
		perror("proot -- ptrace(POKEUSER)");
		exit(EXIT_FAILURE);
	}
}

#if 0

#define AT_FD(dirfd, path) ((dirfd) != AT_FDCWD && ((path) != NULL && (path)[0] != '/'))

void XXX(int syscall_number, ...)
{
	int flags    = -1;
	int dirfd    = -1;
	int olddirfd = -1;
	int newdirfd = -1;
	const char *path    = NULL;
	const char *oldpath = NULL;
	const char *newpath = NULL;

	switch (syscall_number) {
	case SYS__llseek:
	case SYS__newselect:
	case SYS__sysctl:
	case SYS_accept:
	case SYS_accept4:
	case SYS_add_key:
	case SYS_adjtimex:
	case SYS_afs_syscall:
	case SYS_alarm:
	case SYS_arch_prctl:
	case SYS_bdflush:
	case SYS_bind:
	case SYS_break:
	case SYS_brk:
	case SYS_capget:
	case SYS_capset:
	case SYS_clock_getres:
	case SYS_clock_gettime:
	case SYS_clock_nanosleep:
	case SYS_clock_settime:
	case SYS_clone:
	case SYS_close:
	case SYS_connect:
	case SYS_create_module:
	case SYS_delete_module:
	case SYS_dup:
	case SYS_dup2:
	case SYS_dup3:
	case SYS_epoll_create:
	case SYS_epoll_create1:
	case SYS_epoll_ctl:
	case SYS_epoll_ctl_old:
	case SYS_epoll_pwait:
	case SYS_epoll_wait:
	case SYS_epoll_wait_old:
	case SYS_eventfd:
	case SYS_eventfd2:
	case SYS_exit:
	case SYS_exit_group:
	case SYS_fadvise64:
	case SYS_fadvise64_64:
	case SYS_fallocate:
	case SYS_fchdir:
	case SYS_fchmod:
	case SYS_fchown:
	case SYS_fchown32:
	case SYS_fcntl:
	case SYS_fcntl64:
	case SYS_fdatasync:
	case SYS_fgetxattr:
	case SYS_flistxattr:
	case SYS_flock:
	case SYS_fork:
	case SYS_fremovexattr:
	case SYS_fsetxattr:
	case SYS_fstat:
	case SYS_fstat64:
	case SYS_fstatfs:
	case SYS_fstatfs64:
	case SYS_fsync:
	case SYS_ftime:
	case SYS_ftruncate:
	case SYS_ftruncate64:
	case SYS_futex:
	case SYS_get_kernel_syms:
	case SYS_get_mempolicy:
	case SYS_get_robust_list:
	case SYS_get_thread_area:
	case SYS_getcpu:
	case SYS_getdents:
	case SYS_getdents64:
	case SYS_getegid:
	case SYS_getegid32:
	case SYS_geteuid:
	case SYS_geteuid32:
	case SYS_getgid:
	case SYS_getgid32:
	case SYS_getgroups:
	case SYS_getgroups32:
	case SYS_getitimer:
	case SYS_getpeername:
	case SYS_getpgid:
	case SYS_getpgrp:
	case SYS_getpid:
	case SYS_getpmsg:
	case SYS_getppid:
	case SYS_getpriority:
	case SYS_getresgid:
	case SYS_getresgid32:
	case SYS_getresuid:
	case SYS_getresuid32:
	case SYS_getrlimit:
	case SYS_getrusage:
	case SYS_getsid:
	case SYS_getsockname:
	case SYS_getsockopt:
	case SYS_gettid:
	case SYS_gettimeofday:
	case SYS_getuid:
	case SYS_getuid32:
	case SYS_gtty:
	case SYS_idle:
	case SYS_init_module:
	case SYS_inotify_init:
	case SYS_inotify_init1:
	case SYS_inotify_rm_watch:
	case SYS_io_cancel:
	case SYS_io_destroy:
	case SYS_io_getevents:
	case SYS_io_setup:
	case SYS_io_submit:
	case SYS_ioctl:
	case SYS_ioperm:
	case SYS_iopl:
	case SYS_ioprio_get:
	case SYS_ioprio_set:
	case SYS_ipc:
	case SYS_kexec_load:
	case SYS_keyctl:
	case SYS_kill:
	case SYS_listen:
	case SYS_lock:
	case SYS_lookup_dcookie:
	case SYS_lseek:
	case SYS_madvise:
	case SYS_madvise1:
	case SYS_mbind:
	case SYS_migrate_pages:
	case SYS_mincore:
	case SYS_mlock:
	case SYS_mlockall:
	case SYS_mmap:
	case SYS_mmap2:
	case SYS_modify_ldt:
	case SYS_move_pages:
	case SYS_mprotect:
	case SYS_mpx:
	case SYS_mq_getsetattr:
	case SYS_mq_notify:
	case SYS_mq_open:
	case SYS_mq_timedreceive:
	case SYS_mq_timedsend:
	case SYS_mq_unlink:
	case SYS_mremap:
	case SYS_msgctl:
	case SYS_msgget:
	case SYS_msgrcv:
	case SYS_msgsnd:
	case SYS_msync:
	case SYS_munlock:
	case SYS_munlockall:
	case SYS_munmap:
	case SYS_nanosleep:
	case SYS_nfsservctl:
	case SYS_nice:
	case SYS_oldfstat:
	case SYS_oldolduname:
	case SYS_olduname:
	case SYS_pause:
	case SYS_perf_event_open:
	case SYS_personality:
	case SYS_pipe:
	case SYS_pipe2:
	case SYS_poll:
	case SYS_ppoll:
	case SYS_prctl:
	case SYS_pread64:
	case SYS_preadv:
	case SYS_prof:
	case SYS_profil:
	case SYS_pselect6:
	case SYS_ptrace: /* Here comes the exit door ;) Do NOT use PRoot for security purpose! */
	case SYS_putpmsg:
	case SYS_pwrite64:
	case SYS_pwritev:
	case SYS_query_module:
	case SYS_quotactl:
	case SYS_read:
	case SYS_readahead:
	case SYS_readdir:
	case SYS_readv:
	case SYS_reboot:
	case SYS_recvfrom:
	case SYS_recvmmsg:
	case SYS_recvmsg:
	case SYS_remap_file_pages:
	case SYS_request_key:
	case SYS_restart_syscall:
	case SYS_rt_sigaction:
	case SYS_rt_sigpending:
	case SYS_rt_sigprocmask:
	case SYS_rt_sigqueueinfo:
	case SYS_rt_sigreturn:
	case SYS_rt_sigsuspend:
	case SYS_rt_sigtimedwait:
	case SYS_rt_tgsigqueueinfo:
	case SYS_sched_get_priority_max:
	case SYS_sched_get_priority_min:
	case SYS_sched_getaffinity:
	case SYS_sched_getparam:
	case SYS_sched_getscheduler:
	case SYS_sched_rr_get_interval:
	case SYS_sched_setaffinity:
	case SYS_sched_setparam:
	case SYS_sched_setscheduler:
	case SYS_sched_yield:
	case SYS_security:
	case SYS_select:
	case SYS_semctl:
	case SYS_semget:
	case SYS_semop:
	case SYS_semtimedop:
	case SYS_sendfile:
	case SYS_sendfile64:
	case SYS_sendmsg:
	case SYS_sendto:
	case SYS_set_mempolicy:
	case SYS_set_robust_list:
	case SYS_set_thread_area:
	case SYS_set_tid_address:
	case SYS_setdomainname:
	case SYS_setfsgid:
	case SYS_setfsgid32:
	case SYS_setfsuid:
	case SYS_setfsuid32:
	case SYS_setgid:
	case SYS_setgid32:
	case SYS_setgroups:
	case SYS_setgroups32:
	case SYS_sethostname:
	case SYS_setitimer:
	case SYS_setpgid:
	case SYS_setpriority:
	case SYS_setregid:
	case SYS_setregid32:
	case SYS_setresgid:
	case SYS_setresgid32:
	case SYS_setresuid:
	case SYS_setresuid32:
	case SYS_setreuid:
	case SYS_setreuid32:
	case SYS_setrlimit:
	case SYS_setsid:
	case SYS_setsockopt:
	case SYS_settimeofday:
	case SYS_setuid:
	case SYS_setuid32:
	case SYS_sgetmask:
	case SYS_shmat:
	case SYS_shmctl:
	case SYS_shmdt:
	case SYS_shmget:
	case SYS_shutdown:
	case SYS_sigaction:
	case SYS_sigaltstack:
	case SYS_signal:
	case SYS_signalfd:
	case SYS_signalfd4:
	case SYS_sigpending:
	case SYS_sigprocmask:
	case SYS_sigreturn:
	case SYS_sigsuspend:
	case SYS_socket:
	case SYS_socketcall:
	case SYS_socketpair:
	case SYS_splice:
	case SYS_ssetmask:
	case SYS_stime:
	case SYS_stty:
	case SYS_sync:
	case SYS_sync_file_range:
	case SYS_sysfs:
	case SYS_sysinfo:
	case SYS_syslog:
	case SYS_tee:
	case SYS_tgkill:
	case SYS_time:
	case SYS_timer_create:
	case SYS_timer_delete:
	case SYS_timer_getoverrun:
	case SYS_timer_gettime:
	case SYS_timer_settime:
	case SYS_timerfd_create:
	case SYS_timerfd_gettime:
	case SYS_timerfd_settime:
	case SYS_times:
	case SYS_tkill:
	case SYS_tuxcall:
	case SYS_ugetrlimit:
	case SYS_ulimit:
	case SYS_umask:
	case SYS_uname:
	case SYS_unshare:
	case SYS_ustat:
	case SYS_vfork:
	case SYS_vhangup:
	case SYS_vm86:
	case SYS_vm86old:
	case SYS_vmsplice:
	case SYS_vserver:
	case SYS_wait4:
	case SYS_waitid:
	case SYS_waitpid:
	case SYS_write:
	case SYS_writev:
		return;

	case SYS_access:
	case SYS_acct:
	case SYS_chdir:
	case SYS_chmod:
	case SYS_chown:
	case SYS_chown32:
	case SYS_chroot:
	case SYS_execve:
	case SYS_getxattr:
	case SYS_listxattr:
	case SYS_mkdir:
	case SYS_mknod:
	case SYS_oldstat:
	case SYS_creat:
	case SYS_removexattr:
	case SYS_rmdir:
	case SYS_setxattr:
	case SYS_stat:
	case SYS_stat64:
	case SYS_statfs:
	case SYS_statfs64:
	case SYS_swapoff:
	case SYS_swapon:
	case SYS_truncate:
	case SYS_truncate64:
	case SYS_unlink:
	case SYS_uselib:
	case SYS_utime:
	case SYS_utimes:
		change_args(IN_1_REGULAR);
		return;

	case SYS_open:
		flags = get_arg(2);
		if ((flags & O_NOFOLLOW) != 0
		    || (flags & O_EXCL) != 0 && (flags & O_CREAT) != 0)
			change_args(IN_1_SYMLINK);
		else
			change_args(IN_1_REGULAR);
		return;

	case SYS_faccessat:
	case SYS_fchmodat:
	case SYS_fchownat:
	case SYS_fstatat64:
	case SYS_newfstatat:
	case SYS_utimensat:
		dirfd = get_arg(1);
		path  = get_arg(2);
		flags = (syscall_number == SYS_fchownat) ? get_arg(5) : get_arg(4);

		if (!AT_FD(dirfd, path)) {
			if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
				change_args(IN_2_SYMLINK);
			else
				change_args(IN_2_REGULAR);
		}
		else {
			if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
				check_at(dirfd, path, SYMLINK);
			else
				check_at(dirfd, path, REGULAR);
		}
		return;

	case SYS_futimesat:
	case SYS_mkdirat:
	case SYS_mknodat:
	case SYS_unlinkat:
		dirfd = get_arg(1);
		path  = get_arg(2);
		if (!AT_FD(dirfd, path))
			change_args(IN_2_REGULAR);
		else
			check_at(dirfd, path, REGULAR);
		return;

	case SYS_getcwd:
		change_args(OUT_0_REGULAR);
		return;

	case SYS_inotify_add_watch:
		flags = get_arg(3);
		if ((flags & IN_DONT_FOLLOW) != 0)
			change_args(IN_2_SYMLINK);
		else
			change_args(IN_2_REGULAR);
		return;

	case SYS_lchown:
	case SYS_lchown32:
	case SYS_lgetxattr:
	case SYS_llistxattr:
	case SYS_lremovexattr:
	case SYS_lsetxattr:
	case SYS_lstat:
	case SYS_lstat64:
	case SYS_oldlstat:
		change_args(IN_1_SYMLINK);
		return;

	case SYS_link:
	case SYS_pivot_root:
		change_args(IN_1_REGULAR | IN_2_REGULAR);
		return;

	case SYS_linkat:
		olddirfd = get_arg(1);
		oldpath  = get_arg(2);
		newdirfd = get_arg(3);
		newpath  = get_arg(4);
		flags    = get_arg(5);

		if (!AT_FD(olddirfd, oldpath) && !AT_FD(newdirfd, newpath)) {
			if ((flags & AT_SYMLINK_FOLLOW) != 0)
				change_args(IN_2_REGULAR | IN_4_REGULAR);
			else
				change_args(IN_2_SYMLINK | IN_4_REGULAR);
		}
		else if (!AT_FD(olddirfd, oldpath) && AT_FD(newdirfd, newpath)) {
			if ((flags & AT_SYMLINK_FOLLOW) != 0)
				change_args(IN_2_REGULAR);
			else
				change_args(IN_2_SYMLINK);
			check_at(newdirfd, newpath, REGULAR);
		}
		else if (AT_FD(olddirfd, oldpath) && !AT_FD(newdirfd, newpath)) {
			if ((flags & AT_SYMLINK_FOLLOW) != 0)
				check_at(olddirfd, oldpath, REGULAR);
			else
				check_at(olddirfd, oldpath, SYMLINK);
			change_args(IN_4_REGULAR);
		}
		else {
			if ((flags & AT_SYMLINK_FOLLOW) != 0)
				check_at(olddirfd, oldpath, REGULAR);
			else
				check_at(olddirfd, oldpath, SYMLINK);
			check_at(newdirfd, newpath, REGULAR);
		}
		return;

	case SYS_mount:
	case SYS_umount:
	case SYS_umount2:
		path = get_arg(1);
		if (path != NULL && (path[0] == '/' || path[0] == '.'))
			change_args(IN_2_REGULAR);
		return;

	case SYS_openat:
		dirfd = get_arg(1);
		path  = get_arg(2);

		if (!AT_FD(dirfd, path)) {
			flags = get_arg(3);

			if ((flags & O_NOFOLLOW) != 0
			    || (flags & O_EXCL) != 0 && (flags & O_CREAT) != 0)
				change_args(IN_2_SYMLINK);
			else
				change_args(IN_2_REGULAR);
		}
		else
			check_at(dirfd, path);
		return;

	case SYS_readlink:
		change_args(IN_1_SYMLINK | OUT_2_SYMLINK);
		return ;

	case SYS_readlinkat:
		dirfd = get_arg(1);
		path  = get_arg(2);
		if (!AT_FD(dirfd, path))
			change_args(IN_1_SYMLINK | OUT_2_SYMLINK);
		else
			check_at(dirfd, path, SYMLINK);
		return;

	case SYS_rename:
		change_args(IN_1_SYMLINK | IN_2_SYMLINK);
		return;

	case SYS_renameat:
		olddirfd = get_arg(1);
		oldpath  = get_arg(2);
		newdirfd = get_arg(3);
		newpath  = get_arg(4);

		if (!AT_FD(olddirfd, oldpath) && !AT_FD(newdirfd, newpath)) {
			change_args(IN_2_SYMLINK | IN_4_SYMLINK);
		}
		else if (!AT_FD(olddirfd, oldpath) && AT_FD(newdirfd, newpath)) {
			change_args(IN_2_SYMLINK);
			check_at(newdirfd, newpath, SYMLINK);
		}
		else if (AT_FD(olddirfd, oldpath) && !AT_FD(newdirfd, newpath)) {
			check_at(olddirfd, oldpath, SYMLINK);
			change_args(IN_4_SYMLINK);
		}
		else {
			check_at(olddirfd, oldpath, SYMLINK);
			check_at(newdirfd, newpath, SYMLINK);
		}
		return;

	case SYS_symlink:
		change_args(IN_4_REGULAR);
		return;

	case SYS_symlinkat:
		newdirfd = getarg(2);
		newpath  = getarg(3);
		if (!AT_FD(newdirfd, newpath))
			change_args(IN_2_REGULAR);
		else
			check_at(dirfd, path, REGULAR);
		return;

	default:
		fprintf("proot: unknown syscall %d\n", syscall_number);
		return;
	}
}

#endif /* 0 */
