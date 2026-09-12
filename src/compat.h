/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#ifndef COMPAT_H
#define COMPAT_H

/* Local definitions for compatibility with old and/or broken distros... */
#ifndef AT_NULL
#define AT_NULL			0
#endif
#ifndef AT_PHDR
#define AT_PHDR			3
#endif
#ifndef AT_PHENT
#define AT_PHENT		4
#endif
#ifndef AT_PHNUM
#define AT_PHNUM		5
#endif
#ifndef AT_BASE
#define AT_BASE			7
#endif
#ifndef AT_ENTRY
#define AT_ENTRY		9
#endif
#ifndef AT_RANDOM
#define AT_RANDOM		25
#endif
#ifndef AT_EXECFN
#define AT_EXECFN		31
#endif
#ifndef AT_SYSINFO
#define AT_SYSINFO		32
#endif
#ifndef AT_SYSINFO_EHDR
#define AT_SYSINFO_EHDR		33
#endif
#ifndef AT_FDCWD
#define AT_FDCWD		-100
#endif
#ifndef AT_SYMLINK_FOLLOW
#define AT_SYMLINK_FOLLOW	0x400
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR		0x200
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW	0x100
#endif
#ifndef IN_DONT_FOLLOW
#define IN_DONT_FOLLOW		0x02000000
#endif
#ifndef WIFCONTINUED
#define WIFCONTINUED(status)	((status) == 0xffff)
#endif
#ifndef PTRACE_GETREGS
#define PTRACE_GETREGS		12
#endif
#ifndef PTRACE_SETREGS
#define PTRACE_SETREGS		13
#endif
#ifndef PTRACE_GETFPREGS
#define PTRACE_GETFPREGS	14
#endif
#ifndef PTRACE_SETFPREGS
#define PTRACE_SETFPREGS	15
#endif
#ifndef PTRACE_GETFPXREGS
#define PTRACE_GETFPXREGS	18
#endif
#ifndef PTRACE_SETFPXREGS
#define PTRACE_SETFPXREGS	19
#endif
#ifndef PTRACE_SETOPTIONS
#define PTRACE_SETOPTIONS	0x4200
#endif
#ifndef PTRACE_GETEVENTMSG
#define PTRACE_GETEVENTMSG	0x4201
#endif
#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET	0x4204
#endif
#ifndef PTRACE_SETREGSET
#define PTRACE_SETREGSET	0x4205
#endif
#ifndef PTRACE_SEIZE
#define PTRACE_SEIZE		0x4206
#endif
#ifndef PTRACE_INTERRUPT
#define PTRACE_INTERRUPT	0x4207
#endif
#ifndef PTRACE_LISTEN
#define PTRACE_LISTEN		0x4208
#endif
#ifndef PTRACE_O_TRACESYSGOOD
#define PTRACE_O_TRACESYSGOOD	0x00000001
#endif
#ifndef PTRACE_O_TRACEFORK
#define PTRACE_O_TRACEFORK	0x00000002
#endif
#ifndef PTRACE_O_TRACEVFORK
#define PTRACE_O_TRACEVFORK	0x00000004
#endif
#ifndef PTRACE_O_TRACECLONE
#define PTRACE_O_TRACECLONE	0x00000008
#endif
#ifndef PTRACE_O_TRACEEXEC
#define PTRACE_O_TRACEEXEC	0x00000010
#endif
#ifndef PTRACE_O_TRACEVFORKDONE
#define PTRACE_O_TRACEVFORKDONE	0x00000020
#endif
#ifndef PTRACE_O_TRACEEXIT
#define PTRACE_O_TRACEEXIT	0x00000040
#endif
#ifndef PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACESECCOMP	0x00000080
#endif
#ifndef PTRACE_EVENT_FORK
#define PTRACE_EVENT_FORK	1
#endif
#ifndef PTRACE_EVENT_VFORK
#define PTRACE_EVENT_VFORK	2
#endif
#ifndef PTRACE_EVENT_CLONE
#define PTRACE_EVENT_CLONE	3
#endif
#ifndef PTRACE_EVENT_EXEC
#define PTRACE_EVENT_EXEC	4
#endif
#ifndef PTRACE_EVENT_VFORK_DONE
#define PTRACE_EVENT_VFORK_DONE	5
#endif
#ifndef PTRACE_EVENT_EXIT
#define PTRACE_EVENT_EXIT	6
#endif
#ifndef PTRACE_EVENT_SECCOMP
#define PTRACE_EVENT_SECCOMP	7
#endif
#ifndef PTRACE_EVENT_SECCOMP2
#if PTRACE_EVENT_SECCOMP == 7
#define PTRACE_EVENT_SECCOMP2	8
#elif PTRACE_EVENT_SECCOMP == 8
#define PTRACE_EVENT_SECCOMP2	7
#else
#error "unknown PTRACE_EVENT_SECCOMP value"
#endif
#endif
#ifndef PTRACE_SET_SYSCALL
#define PTRACE_SET_SYSCALL	23
#endif
#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA	25
#endif
#ifndef PTRACE_SET_THREAD_AREA
#define PTRACE_SET_THREAD_AREA	26
#endif
#ifndef PTRACE_GETVFPREGS
#define PTRACE_GETVFPREGS	27
#endif
#ifndef PTRACE_ARCH_PRCTL
#define PTRACE_ARCH_PRCTL	30
#endif
#ifndef ARCH_SET_GS
#define ARCH_SET_GS	0x1001
#endif
#ifndef ARCH_SET_FS
#define ARCH_SET_FS	0x1002
#endif
#ifndef ARCH_GET_GS
#define ARCH_GET_FS	0x1003
#endif
#ifndef ARCH_GET_FS
#define ARCH_GET_GS	0x1004
#endif
#ifndef PTRACE_SINGLEBLOCK
#define PTRACE_SINGLEBLOCK	33
#endif
#ifndef ADDR_NO_RANDOMIZE
#define ADDR_NO_RANDOMIZE	0x0040000
#endif
#ifndef SYS_ACCEPT4
#define SYS_ACCEPT4		18
#endif
#ifndef TALLOC_FREE
#define TALLOC_FREE(ctx) do { talloc_free(ctx); ctx = NULL; } while(0)
#endif
#ifndef PR_SET_NAME
#define PR_SET_NAME		15
#endif
#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS	38
#endif
#ifndef PR_SET_SECCOMP
#define PR_SET_SECCOMP		22
#endif
#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_FILTER	2
#endif
#ifndef talloc_get_type_abort
#define talloc_get_type_abort talloc_get_type
#endif
#ifndef FUTEX_PRIVATE_FLAG
#define FUTEX_PRIVATE_FLAG	128
#endif
#ifndef EFD_SEMAPHORE
#define EFD_SEMAPHORE		1
#endif
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC		1030
#endif
#ifndef O_RDONLY
#define O_RDONLY		00000000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC		02000000
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE			0x02
#endif
#ifndef MAP_FIXED
#define MAP_FIXED			0x10
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS			0x20
#endif
#ifndef PROT_READ
#define PROT_READ		0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE		0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC		0x4
#endif
#ifndef PROT_GROWSDOWN
#define PROT_GROWSDOWN		0x01000000
#endif
#ifndef NT_ARM_SYSTEM_CALL
#define NT_ARM_SYSTEM_CALL		0x404
#endif

/* compat.h is also pulled in by the freestanding, -nostdlib loader
 * build (see src/GNUmakefile's build_loader), which has no libc
 * available at all -- <sys/user.h> may not even exist there (e.g. no
 * 32-bit multilib headers for the m32 loader). The register-transfer
 * structs below are only ever needed by hosted code that does ptrace
 * (tracee.h / ptrace.c); the loader never touches them. Skip this
 * whole block for freestanding translation units.  */
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__

/* Some older glibc (e.g. 2.17, as shipped by RHEL/CentOS 7's initial
 * aarch64 port) never added struct user_regs_struct as a source
 * compatibility alias for the kernel's struct user_pt_regs: the name
 * is forward-declared by <sys/user.h> but never given a member list,
 * so it stays an incomplete type and any use of it fails to compile
 * ("array type has incomplete element type", "incomplete type is not
 * allowed", ...).  Provide it ourselves here, laid out exactly like
 * the kernel ABI (arch/arm64/include/uapi/asm/ptrace.h), for the
 * glibc versions that need it.  */
#include <sys/user.h>
#if defined(__aarch64__) && defined(__GLIBC__)
#if !__GLIBC_PREREQ(2, 24)
struct user_regs_struct {
    unsigned long long regs[31];
    unsigned long long sp;
    unsigned long long pc;
    unsigned long long pstate;
};
#endif
#endif

/* Unlike struct user_regs_struct above, glibc has never defined
 * struct user_fpregs_struct for aarch64 (confirmed missing as of
 * glibc 2.39): the kernel's FP/SIMD state has always only had a
 * name of its own, struct user_fpsimd_state.  Provide the x86-style
 * alias ourselves, laid out exactly like the kernel ABI (see
 * arch/arm64/include/uapi/asm/ptrace.h), so this doesn't depend on
 * a glibc version ever adding it.  */
#if defined(__aarch64__)
struct user_fpregs_struct {
    unsigned __int128 vregs[32];
    unsigned int fpsr;
    unsigned int fpcr;
    unsigned int __reserved[2];
};
#endif

#endif				/* !defined(__STDC_HOSTED__) || __STDC_HOSTED__ */

#endif				/* COMPAT_H */
