/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file was generated thanks to the following command:
 *
 *     cpp -dM linux/arch/x86/syscalls/../include/generated/asm/unistd_x32.h | grep '#define __NR_' | sed s/__NR_/PR_/g | sort -u
 */

#include "syscall/sysnum-undefined.h"

#define PR_X32_SYSCALL_BIT 0x40000000


#define PR_accept (PR_X32_SYSCALL_BIT + 43)
#define PR_accept4 (PR_X32_SYSCALL_BIT + 288)
#define PR_access (PR_X32_SYSCALL_BIT + 21)
#define PR_acct (PR_X32_SYSCALL_BIT + 163)
#define PR_add_key (PR_X32_SYSCALL_BIT + 248)
#define PR_adjtimex (PR_X32_SYSCALL_BIT + 159)
#define PR_afs_syscall (PR_X32_SYSCALL_BIT + 183)
#define PR_alarm (PR_X32_SYSCALL_BIT + 37)
#define PR_arch_prctl (PR_X32_SYSCALL_BIT + 158)
#define PR_bind (PR_X32_SYSCALL_BIT + 49)
#define PR_brk (PR_X32_SYSCALL_BIT + 12)
#define PR_capget (PR_X32_SYSCALL_BIT + 125)
#define PR_capset (PR_X32_SYSCALL_BIT + 126)
#define PR_chdir (PR_X32_SYSCALL_BIT + 80)
#define PR_chmod (PR_X32_SYSCALL_BIT + 90)
#define PR_chown (PR_X32_SYSCALL_BIT + 92)
#define PR_chroot (PR_X32_SYSCALL_BIT + 161)
#define PR_clock_adjtime (PR_X32_SYSCALL_BIT + 305)
#define PR_clock_getres (PR_X32_SYSCALL_BIT + 229)
#define PR_clock_gettime (PR_X32_SYSCALL_BIT + 228)
#define PR_clock_nanosleep (PR_X32_SYSCALL_BIT + 230)
#define PR_clock_settime (PR_X32_SYSCALL_BIT + 227)
#define PR_clone (PR_X32_SYSCALL_BIT + 56)
#define PR_close (PR_X32_SYSCALL_BIT + 3)
#define PR_connect (PR_X32_SYSCALL_BIT + 42)
#define PR_creat (PR_X32_SYSCALL_BIT + 85)
#define PR_delete_module (PR_X32_SYSCALL_BIT + 176)
#define PR_dup (PR_X32_SYSCALL_BIT + 32)
#define PR_dup2 (PR_X32_SYSCALL_BIT + 33)
#define PR_dup3 (PR_X32_SYSCALL_BIT + 292)
#define PR_epoll_create (PR_X32_SYSCALL_BIT + 213)
#define PR_epoll_create1 (PR_X32_SYSCALL_BIT + 291)
#define PR_epoll_ctl (PR_X32_SYSCALL_BIT + 233)
#define PR_epoll_pwait (PR_X32_SYSCALL_BIT + 281)
#define PR_epoll_wait (PR_X32_SYSCALL_BIT + 232)
#define PR_eventfd (PR_X32_SYSCALL_BIT + 284)
#define PR_eventfd2 (PR_X32_SYSCALL_BIT + 290)
#define PR_execve (PR_X32_SYSCALL_BIT + 520)
#define PR_exit (PR_X32_SYSCALL_BIT + 60)
#define PR_exit_group (PR_X32_SYSCALL_BIT + 231)
#define PR_faccessat (PR_X32_SYSCALL_BIT + 269)
#define PR_fadvise64 (PR_X32_SYSCALL_BIT + 221)
#define PR_fallocate (PR_X32_SYSCALL_BIT + 285)
#define PR_fanotify_init (PR_X32_SYSCALL_BIT + 300)
#define PR_fanotify_mark (PR_X32_SYSCALL_BIT + 301)
#define PR_fchdir (PR_X32_SYSCALL_BIT + 81)
#define PR_fchmod (PR_X32_SYSCALL_BIT + 91)
#define PR_fchmodat (PR_X32_SYSCALL_BIT + 268)
#define PR_fchown (PR_X32_SYSCALL_BIT + 93)
#define PR_fchownat (PR_X32_SYSCALL_BIT + 260)
#define PR_fcntl (PR_X32_SYSCALL_BIT + 72)
#define PR_fdatasync (PR_X32_SYSCALL_BIT + 75)
#define PR_fgetxattr (PR_X32_SYSCALL_BIT + 193)
#define PR_flistxattr (PR_X32_SYSCALL_BIT + 196)
#define PR_flock (PR_X32_SYSCALL_BIT + 73)
#define PR_fork (PR_X32_SYSCALL_BIT + 57)
#define PR_fremovexattr (PR_X32_SYSCALL_BIT + 199)
#define PR_fsetxattr (PR_X32_SYSCALL_BIT + 190)
#define PR_fstat (PR_X32_SYSCALL_BIT + 5)
#define PR_fstatfs (PR_X32_SYSCALL_BIT + 138)
#define PR_fsync (PR_X32_SYSCALL_BIT + 74)
#define PR_ftruncate (PR_X32_SYSCALL_BIT + 77)
#define PR_futex (PR_X32_SYSCALL_BIT + 202)
#define PR_futimesat (PR_X32_SYSCALL_BIT + 261)
#define PR_get_mempolicy (PR_X32_SYSCALL_BIT + 239)
#define PR_get_robust_list (PR_X32_SYSCALL_BIT + 531)
#define PR_getcpu (PR_X32_SYSCALL_BIT + 309)
#define PR_getcwd (PR_X32_SYSCALL_BIT + 79)
#define PR_getdents (PR_X32_SYSCALL_BIT + 78)
#define PR_getdents64 (PR_X32_SYSCALL_BIT + 217)
#define PR_getegid (PR_X32_SYSCALL_BIT + 108)
#define PR_geteuid (PR_X32_SYSCALL_BIT + 107)
#define PR_getgid (PR_X32_SYSCALL_BIT + 104)
#define PR_getgroups (PR_X32_SYSCALL_BIT + 115)
#define PR_getitimer (PR_X32_SYSCALL_BIT + 36)
#define PR_getpeername (PR_X32_SYSCALL_BIT + 52)
#define PR_getpgid (PR_X32_SYSCALL_BIT + 121)
#define PR_getpgrp (PR_X32_SYSCALL_BIT + 111)
#define PR_getpid (PR_X32_SYSCALL_BIT + 39)
#define PR_getpmsg (PR_X32_SYSCALL_BIT + 181)
#define PR_getppid (PR_X32_SYSCALL_BIT + 110)
#define PR_getpriority (PR_X32_SYSCALL_BIT + 140)
#define PR_getresgid (PR_X32_SYSCALL_BIT + 120)
#define PR_getresuid (PR_X32_SYSCALL_BIT + 118)
#define PR_getrlimit (PR_X32_SYSCALL_BIT + 97)
#define PR_getrusage (PR_X32_SYSCALL_BIT + 98)
#define PR_getsid (PR_X32_SYSCALL_BIT + 124)
#define PR_getsockname (PR_X32_SYSCALL_BIT + 51)
#define PR_getsockopt (PR_X32_SYSCALL_BIT + 542)
#define PR_gettid (PR_X32_SYSCALL_BIT + 186)
#define PR_gettimeofday (PR_X32_SYSCALL_BIT + 96)
#define PR_getuid (PR_X32_SYSCALL_BIT + 102)
#define PR_getxattr (PR_X32_SYSCALL_BIT + 191)
#define PR_init_module (PR_X32_SYSCALL_BIT + 175)
#define PR_inotify_add_watch (PR_X32_SYSCALL_BIT + 254)
#define PR_inotify_init (PR_X32_SYSCALL_BIT + 253)
#define PR_inotify_init1 (PR_X32_SYSCALL_BIT + 294)
#define PR_inotify_rm_watch (PR_X32_SYSCALL_BIT + 255)
#define PR_io_cancel (PR_X32_SYSCALL_BIT + 210)
#define PR_io_destroy (PR_X32_SYSCALL_BIT + 207)
#define PR_io_getevents (PR_X32_SYSCALL_BIT + 208)
#define PR_io_setup (PR_X32_SYSCALL_BIT + 206)
#define PR_io_submit (PR_X32_SYSCALL_BIT + 209)
#define PR_ioctl (PR_X32_SYSCALL_BIT + 514)
#define PR_ioperm (PR_X32_SYSCALL_BIT + 173)
#define PR_iopl (PR_X32_SYSCALL_BIT + 172)
#define PR_ioprio_get (PR_X32_SYSCALL_BIT + 252)
#define PR_ioprio_set (PR_X32_SYSCALL_BIT + 251)
#define PR_kcmp (PR_X32_SYSCALL_BIT + 312)
#define PR_kexec_load (PR_X32_SYSCALL_BIT + 528)
#define PR_keyctl (PR_X32_SYSCALL_BIT + 250)
#define PR_kill (PR_X32_SYSCALL_BIT + 62)
#define PR_lchown (PR_X32_SYSCALL_BIT + 94)
#define PR_lgetxattr (PR_X32_SYSCALL_BIT + 192)
#define PR_link (PR_X32_SYSCALL_BIT + 86)
#define PR_linkat (PR_X32_SYSCALL_BIT + 265)
#define PR_listen (PR_X32_SYSCALL_BIT + 50)
#define PR_listxattr (PR_X32_SYSCALL_BIT + 194)
#define PR_llistxattr (PR_X32_SYSCALL_BIT + 195)
#define PR_lookup_dcookie (PR_X32_SYSCALL_BIT + 212)
#define PR_lremovexattr (PR_X32_SYSCALL_BIT + 198)
#define PR_lseek (PR_X32_SYSCALL_BIT + 8)
#define PR_lsetxattr (PR_X32_SYSCALL_BIT + 189)
#define PR_lstat (PR_X32_SYSCALL_BIT + 6)
#define PR_madvise (PR_X32_SYSCALL_BIT + 28)
#define PR_mbind (PR_X32_SYSCALL_BIT + 237)
#define PR_migrate_pages (PR_X32_SYSCALL_BIT + 256)
#define PR_mincore (PR_X32_SYSCALL_BIT + 27)
#define PR_mkdir (PR_X32_SYSCALL_BIT + 83)
#define PR_mkdirat (PR_X32_SYSCALL_BIT + 258)
#define PR_mknod (PR_X32_SYSCALL_BIT + 133)
#define PR_mknodat (PR_X32_SYSCALL_BIT + 259)
#define PR_mlock (PR_X32_SYSCALL_BIT + 149)
#define PR_mlockall (PR_X32_SYSCALL_BIT + 151)
#define PR_mmap (PR_X32_SYSCALL_BIT + 9)
#define PR_modify_ldt (PR_X32_SYSCALL_BIT + 154)
#define PR_mount (PR_X32_SYSCALL_BIT + 165)
#define PR_move_pages (PR_X32_SYSCALL_BIT + 533)
#define PR_mprotect (PR_X32_SYSCALL_BIT + 10)
#define PR_mq_getsetattr (PR_X32_SYSCALL_BIT + 245)
#define PR_mq_notify (PR_X32_SYSCALL_BIT + 527)
#define PR_mq_open (PR_X32_SYSCALL_BIT + 240)
#define PR_mq_timedreceive (PR_X32_SYSCALL_BIT + 243)
#define PR_mq_timedsend (PR_X32_SYSCALL_BIT + 242)
#define PR_mq_unlink (PR_X32_SYSCALL_BIT + 241)
#define PR_mremap (PR_X32_SYSCALL_BIT + 25)
#define PR_msgctl (PR_X32_SYSCALL_BIT + 71)
#define PR_msgget (PR_X32_SYSCALL_BIT + 68)
#define PR_msgrcv (PR_X32_SYSCALL_BIT + 70)
#define PR_msgsnd (PR_X32_SYSCALL_BIT + 69)
#define PR_msync (PR_X32_SYSCALL_BIT + 26)
#define PR_munlock (PR_X32_SYSCALL_BIT + 150)
#define PR_munlockall (PR_X32_SYSCALL_BIT + 152)
#define PR_munmap (PR_X32_SYSCALL_BIT + 11)
#define PR_name_to_handle_at (PR_X32_SYSCALL_BIT + 303)
#define PR_nanosleep (PR_X32_SYSCALL_BIT + 35)
#define PR_newfstatat (PR_X32_SYSCALL_BIT + 262)
#define PR_open (PR_X32_SYSCALL_BIT + 2)
#define PR_open_by_handle_at (PR_X32_SYSCALL_BIT + 304)
#define PR_openat (PR_X32_SYSCALL_BIT + 257)
#define PR_pause (PR_X32_SYSCALL_BIT + 34)
#define PR_perf_event_open (PR_X32_SYSCALL_BIT + 298)
#define PR_personality (PR_X32_SYSCALL_BIT + 135)
#define PR_pipe (PR_X32_SYSCALL_BIT + 22)
#define PR_pipe2 (PR_X32_SYSCALL_BIT + 293)
#define PR_pivot_root (PR_X32_SYSCALL_BIT + 155)
#define PR_poll (PR_X32_SYSCALL_BIT + 7)
#define PR_ppoll (PR_X32_SYSCALL_BIT + 271)
#define PR_prctl (PR_X32_SYSCALL_BIT + 157)
#define PR_pread64 (PR_X32_SYSCALL_BIT + 17)
#define PR_preadv (PR_X32_SYSCALL_BIT + 534)
#define PR_prlimit64 (PR_X32_SYSCALL_BIT + 302)
#define PR_process_vm_readv (PR_X32_SYSCALL_BIT + 539)
#define PR_process_vm_writev (PR_X32_SYSCALL_BIT + 540)
#define PR_pselect6 (PR_X32_SYSCALL_BIT + 270)
#define PR_ptrace (PR_X32_SYSCALL_BIT + 521)
#define PR_putpmsg (PR_X32_SYSCALL_BIT + 182)
#define PR_pwrite64 (PR_X32_SYSCALL_BIT + 18)
#define PR_pwritev (PR_X32_SYSCALL_BIT + 535)
#define PR_quotactl (PR_X32_SYSCALL_BIT + 179)
#define PR_read (PR_X32_SYSCALL_BIT + 0)
#define PR_readahead (PR_X32_SYSCALL_BIT + 187)
#define PR_readlink (PR_X32_SYSCALL_BIT + 89)
#define PR_readlinkat (PR_X32_SYSCALL_BIT + 267)
#define PR_readv (PR_X32_SYSCALL_BIT + 515)
#define PR_reboot (PR_X32_SYSCALL_BIT + 169)
#define PR_recvfrom (PR_X32_SYSCALL_BIT + 517)
#define PR_recvmmsg (PR_X32_SYSCALL_BIT + 537)
#define PR_recvmsg (PR_X32_SYSCALL_BIT + 519)
#define PR_remap_file_pages (PR_X32_SYSCALL_BIT + 216)
#define PR_removexattr (PR_X32_SYSCALL_BIT + 197)
#define PR_rename (PR_X32_SYSCALL_BIT + 82)
#define PR_renameat (PR_X32_SYSCALL_BIT + 264)
#define PR_request_key (PR_X32_SYSCALL_BIT + 249)
#define PR_restart_syscall (PR_X32_SYSCALL_BIT + 219)
#define PR_rmdir (PR_X32_SYSCALL_BIT + 84)
#define PR_rt_sigaction (PR_X32_SYSCALL_BIT + 512)
#define PR_rt_sigpending (PR_X32_SYSCALL_BIT + 522)
#define PR_rt_sigprocmask (PR_X32_SYSCALL_BIT + 14)
#define PR_rt_sigqueueinfo (PR_X32_SYSCALL_BIT + 524)
#define PR_rt_sigreturn (PR_X32_SYSCALL_BIT + 513)
#define PR_rt_sigsuspend (PR_X32_SYSCALL_BIT + 130)
#define PR_rt_sigtimedwait (PR_X32_SYSCALL_BIT + 523)
#define PR_rt_tgsigqueueinfo (PR_X32_SYSCALL_BIT + 536)
#define PR_sched_get_priority_max (PR_X32_SYSCALL_BIT + 146)
#define PR_sched_get_priority_min (PR_X32_SYSCALL_BIT + 147)
#define PR_sched_getaffinity (PR_X32_SYSCALL_BIT + 204)
#define PR_sched_getparam (PR_X32_SYSCALL_BIT + 143)
#define PR_sched_getscheduler (PR_X32_SYSCALL_BIT + 145)
#define PR_sched_rr_get_interval (PR_X32_SYSCALL_BIT + 148)
#define PR_sched_setaffinity (PR_X32_SYSCALL_BIT + 203)
#define PR_sched_setparam (PR_X32_SYSCALL_BIT + 142)
#define PR_sched_setscheduler (PR_X32_SYSCALL_BIT + 144)
#define PR_sched_yield (PR_X32_SYSCALL_BIT + 24)
#define PR_security (PR_X32_SYSCALL_BIT + 185)
#define PR_select (PR_X32_SYSCALL_BIT + 23)
#define PR_semctl (PR_X32_SYSCALL_BIT + 66)
#define PR_semget (PR_X32_SYSCALL_BIT + 64)
#define PR_semop (PR_X32_SYSCALL_BIT + 65)
#define PR_semtimedop (PR_X32_SYSCALL_BIT + 220)
#define PR_sendfile (PR_X32_SYSCALL_BIT + 40)
#define PR_sendmmsg (PR_X32_SYSCALL_BIT + 538)
#define PR_sendmsg (PR_X32_SYSCALL_BIT + 518)
#define PR_sendto (PR_X32_SYSCALL_BIT + 44)
#define PR_set_mempolicy (PR_X32_SYSCALL_BIT + 238)
#define PR_set_robust_list (PR_X32_SYSCALL_BIT + 530)
#define PR_set_tid_address (PR_X32_SYSCALL_BIT + 218)
#define PR_setdomainname (PR_X32_SYSCALL_BIT + 171)
#define PR_setfsgid (PR_X32_SYSCALL_BIT + 123)
#define PR_setfsuid (PR_X32_SYSCALL_BIT + 122)
#define PR_setgid (PR_X32_SYSCALL_BIT + 106)
#define PR_setgroups (PR_X32_SYSCALL_BIT + 116)
#define PR_sethostname (PR_X32_SYSCALL_BIT + 170)
#define PR_setitimer (PR_X32_SYSCALL_BIT + 38)
#define PR_setns (PR_X32_SYSCALL_BIT + 308)
#define PR_setpgid (PR_X32_SYSCALL_BIT + 109)
#define PR_setpriority (PR_X32_SYSCALL_BIT + 141)
#define PR_setregid (PR_X32_SYSCALL_BIT + 114)
#define PR_setresgid (PR_X32_SYSCALL_BIT + 119)
#define PR_setresuid (PR_X32_SYSCALL_BIT + 117)
#define PR_setreuid (PR_X32_SYSCALL_BIT + 113)
#define PR_setrlimit (PR_X32_SYSCALL_BIT + 160)
#define PR_setsid (PR_X32_SYSCALL_BIT + 112)
#define PR_setsockopt (PR_X32_SYSCALL_BIT + 541)
#define PR_settimeofday (PR_X32_SYSCALL_BIT + 164)
#define PR_setuid (PR_X32_SYSCALL_BIT + 105)
#define PR_setxattr (PR_X32_SYSCALL_BIT + 188)
#define PR_shmat (PR_X32_SYSCALL_BIT + 30)
#define PR_shmctl (PR_X32_SYSCALL_BIT + 31)
#define PR_shmdt (PR_X32_SYSCALL_BIT + 67)
#define PR_shmget (PR_X32_SYSCALL_BIT + 29)
#define PR_shutdown (PR_X32_SYSCALL_BIT + 48)
#define PR_sigaltstack (PR_X32_SYSCALL_BIT + 525)
#define PR_signalfd (PR_X32_SYSCALL_BIT + 282)
#define PR_signalfd4 (PR_X32_SYSCALL_BIT + 289)
#define PR_socket (PR_X32_SYSCALL_BIT + 41)
#define PR_socketpair (PR_X32_SYSCALL_BIT + 53)
#define PR_splice (PR_X32_SYSCALL_BIT + 275)
#define PR_stat (PR_X32_SYSCALL_BIT + 4)
#define PR_statfs (PR_X32_SYSCALL_BIT + 137)
#define PR_swapoff (PR_X32_SYSCALL_BIT + 168)
#define PR_swapon (PR_X32_SYSCALL_BIT + 167)
#define PR_symlink (PR_X32_SYSCALL_BIT + 88)
#define PR_symlinkat (PR_X32_SYSCALL_BIT + 266)
#define PR_sync (PR_X32_SYSCALL_BIT + 162)
#define PR_sync_file_range (PR_X32_SYSCALL_BIT + 277)
#define PR_syncfs (PR_X32_SYSCALL_BIT + 306)
#define PR_sysfs (PR_X32_SYSCALL_BIT + 139)
#define PR_sysinfo (PR_X32_SYSCALL_BIT + 99)
#define PR_syslog (PR_X32_SYSCALL_BIT + 103)
#define PR_tee (PR_X32_SYSCALL_BIT + 276)
#define PR_tgkill (PR_X32_SYSCALL_BIT + 234)
#define PR_time (PR_X32_SYSCALL_BIT + 201)
#define PR_timer_create (PR_X32_SYSCALL_BIT + 526)
#define PR_timer_delete (PR_X32_SYSCALL_BIT + 226)
#define PR_timer_getoverrun (PR_X32_SYSCALL_BIT + 225)
#define PR_timer_gettime (PR_X32_SYSCALL_BIT + 224)
#define PR_timer_settime (PR_X32_SYSCALL_BIT + 223)
#define PR_timerfd_create (PR_X32_SYSCALL_BIT + 283)
#define PR_timerfd_gettime (PR_X32_SYSCALL_BIT + 287)
#define PR_timerfd_settime (PR_X32_SYSCALL_BIT + 286)
#define PR_times (PR_X32_SYSCALL_BIT + 100)
#define PR_tkill (PR_X32_SYSCALL_BIT + 200)
#define PR_truncate (PR_X32_SYSCALL_BIT + 76)
#define PR_tuxcall (PR_X32_SYSCALL_BIT + 184)
#define PR_umask (PR_X32_SYSCALL_BIT + 95)
#define PR_umount2 (PR_X32_SYSCALL_BIT + 166)
#define PR_uname (PR_X32_SYSCALL_BIT + 63)
#define PR_unlink (PR_X32_SYSCALL_BIT + 87)
#define PR_unlinkat (PR_X32_SYSCALL_BIT + 263)
#define PR_unshare (PR_X32_SYSCALL_BIT + 272)
#define PR_ustat (PR_X32_SYSCALL_BIT + 136)
#define PR_utime (PR_X32_SYSCALL_BIT + 132)
#define PR_utimensat (PR_X32_SYSCALL_BIT + 280)
#define PR_utimes (PR_X32_SYSCALL_BIT + 235)
#define PR_vfork (PR_X32_SYSCALL_BIT + 58)
#define PR_vhangup (PR_X32_SYSCALL_BIT + 153)
#define PR_vmsplice (PR_X32_SYSCALL_BIT + 532)
#define PR_wait4 (PR_X32_SYSCALL_BIT + 61)
#define PR_waitid (PR_X32_SYSCALL_BIT + 529)
#define PR_write (PR_X32_SYSCALL_BIT + 1)
#define PR_writev (PR_X32_SYSCALL_BIT + 516)

/*
 * These following syscalls are x32 specific, this list was generated
 * thanks to the following command:
 *
 *     cpp -dM linux/arch/x86/syscalls/../include/generated/asm/unistd_64_x32.h | grep '#define __NR_x32_' | sed s/__NR_x32_/PR_X32_/g | sort -u
 */

#define PR_x32_execve 520
#define PR_x32_get_robust_list 531
#define PR_x32_ioctl 514
#define PR_x32_kexec_load 528
#define PR_x32_move_pages 533
#define PR_x32_mq_notify 527
#define PR_x32_preadv 534
#define PR_x32_process_vm_readv 539
#define PR_x32_process_vm_writev 540
#define PR_x32_ptrace 521
#define PR_x32_pwritev 535
#define PR_x32_readv 515
#define PR_x32_recvfrom 517
#define PR_x32_recvmmsg 537
#define PR_x32_recvmsg 519
#define PR_x32_rt_sigaction 512
#define PR_x32_rt_sigpending 522
#define PR_x32_rt_sigqueueinfo 524
#define PR_x32_rt_sigreturn 513
#define PR_x32_rt_sigtimedwait 523
#define PR_x32_rt_tgsigqueueinfo 536
#define PR_x32_sendmmsg 538
#define PR_x32_sendmsg 518
#define PR_x32_set_robust_list 530
#define PR_x32_sigaltstack 525
#define PR_x32_timer_create 526
#define PR_x32_vmsplice 532
#define PR_x32_waitid 529
#define PR_x32_writev 516

/*
 * These following syscalls do not exist on x32.  Note that syscall
 * numbers from -1 to -10 are reserved for PRoot internal usage.
 */

#define PR__llseek -11
#define PR__newselect -12
#define PR__sysctl -13
#define PR_bdflush -14
#define PR_break -15
#define PR_cacheflush -16
#define PR_chown32 -17
#define PR_create_module -18
#define PR_epoll_ctl_old -19
#define PR_epoll_wait_old -20
#define PR_fadvise64_64 -21
#define PR_fchown32 -22
#define PR_fcntl64 -23
#define PR_fstat64 -24
#define PR_fstatat64 -25
#define PR_fstatfs64 -26
#define PR_ftime -27
#define PR_ftruncate64 -28
#define PR_get_kernel_syms -29
#define PR_get_thread_area -30
#define PR_getegid32 -31
#define PR_geteuid32 -32
#define PR_getgid32 -33
#define PR_getgroups32 -34
#define PR_getresgid32 -35
#define PR_getresuid32 -36
#define PR_getuid32 -37
#define PR_gtty -38
#define PR_idle -39
#define PR_ipc -40
#define PR_lchown32 -41
#define PR_lock -42
#define PR_lstat64 -43
#define PR_mmap2 -44
#define PR_mpx -45
#define PR_nfsservctl -46
#define PR_nice -47
#define PR_oldfstat -48
#define PR_oldlstat -49
#define PR_oldolduname -50
#define PR_oldstat -51
#define PR_olduname -52
#define PR_pciconfig_iobase -53
#define PR_pciconfig_read -54
#define PR_pciconfig_write -55
#define PR_profil -56
#define PR_prof -57
#define PR_query_module -58
#define PR_readdir -59
#define PR_recv -60
#define PR_sendfile64 -61
#define PR_send -62
#define PR_set_thread_area -63
#define PR_setfsgid32 -64
#define PR_setfsuid32 -65
#define PR_setgid32 -66
#define PR_setgroups32 -67
#define PR_setregid32 -68
#define PR_setresgid32 -69
#define PR_setresuid32 -70
#define PR_setreuid32 -71
#define PR_setuid32 -72
#define PR_sgetmask -73
#define PR_sigaction -74
#define PR_signal -75
#define PR_sigpending -76
#define PR_sigprocmask -77
#define PR_sigreturn -78
#define PR_sigsuspend -79
#define PR_socketcall -80
#define PR_ssetmask -81
#define PR_stat64 -82
#define PR_statfs64 -83
#define PR_stime -84
#define PR_stty -85
#define PR_sync_file_range2 -86
#define PR_truncate64 -87
#define PR_ugetrlimit -88
#define PR_ulimit -89
#define PR_umount -90
#define PR_uselib -91
#define PR_vm86old -92
#define PR_vm86 -93
#define PR_vserver -94
#define PR_waitpid -95

