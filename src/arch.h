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

#ifndef ARCH_H
#define ARCH_H

#ifndef NO_LIBC_HEADER
#include <sys/ptrace.h>    /* linux.git:c0a3a20b  */
#include <linux/audit.h>   /* AUDIT_ARCH_*,  */
#endif

typedef unsigned long word_t;
typedef unsigned char byte_t;

#define SYSCALL_AVOIDER ((word_t) -2)
#define SYSTRAP_NUM SYSARG_NUM

#if !defined(ARCH_X86_64) && !defined(ARCH_ARM_EABI) && !defined(ARCH_X86) && !defined(ARCH_SH4)
#    if defined(__x86_64__)
#        define ARCH_X86_64 1
#    elif defined(__ARM_EABI__)
#        define ARCH_ARM_EABI 1
#    elif defined(__aarch64__)
#        define ARCH_ARM64 1
#    elif defined(__arm__)
#        error "Only EABI is currently supported for ARM"
#    elif defined(__i386__)
#        define ARCH_X86 1
#    elif defined(__SH4__)
#        define ARCH_SH4 1
#    else
#        error "Unsupported architecture"
#    endif
#endif

/* Architecture specific definitions. */
#if defined(ARCH_X86_64)

    #define SYSNUMS_HEADER1 "syscall/sysnums-x86_64.h"
    #define SYSNUMS_HEADER2 "syscall/sysnums-i386.h"
    #define SYSNUMS_HEADER3 "syscall/sysnums-x32.h"

    #define SYSNUMS_ABI1 sysnums_x86_64
    #define SYSNUMS_ABI2 sysnums_i386
    #define SYSNUMS_ABI3 sysnums_x32

    #undef  SYSTRAP_NUM
    #define SYSTRAP_NUM SYSARG_RESULT
    #define SYSTRAP_SIZE 2

    #define SECCOMP_ARCHS {									\
		{ .value = AUDIT_ARCH_X86_64, .nb_abis = 2, .abis = { ABI_DEFAULT, ABI_3 } },	\
		{ .value = AUDIT_ARCH_I386,   .nb_abis = 1, .abis = { ABI_2 } }, 		\
	}

    #define HOST_ELF_MACHINE {62, 3, 6, 0}
    #define RED_ZONE_SIZE 128
    #define OFFSETOF_STAT_UID_32 24
    #define OFFSETOF_STAT_GID_32 28

    #define LOADER_ADDRESS 0x600000000000
    #define HAS_LOADER_32BIT true

    #define EXEC_PIC_ADDRESS   0x500000000000
    #define INTERP_PIC_ADDRESS 0x6f0000000000
    #define EXEC_PIC_ADDRESS_32   0x0f000000
    #define INTERP_PIC_ADDRESS_32 0xaf000000

#elif defined(ARCH_ARM_EABI)

    #define SYSNUMS_HEADER1 "syscall/sysnums-arm.h"
    #define SYSNUMS_ABI1    sysnums_arm

    #define SYSTRAP_SIZE 4

    #define SECCOMP_ARCHS { { .value = AUDIT_ARCH_ARM, .nb_abis = 1, .abis = { ABI_DEFAULT } } }

    #define user_regs_struct user_regs
    #define HOST_ELF_MACHINE {40, 0};
    #define RED_ZONE_SIZE 0
    #define OFFSETOF_STAT_UID_32 0
    #define OFFSETOF_STAT_GID_32 0
    #define EM_ARM 40

    #define LOADER_ADDRESS 0x10000000

    #define EXEC_PIC_ADDRESS   0x0f000000
    #define INTERP_PIC_ADDRESS 0x1f000000

    /* The syscall number has to be valid on ARM, so use tuxcall(2) as
     * the "void" syscall since it has no side effects.  */
    #undef SYSCALL_AVOIDER
    #define SYSCALL_AVOIDER ((word_t) 222)

#elif defined(ARCH_ARM64)

    #define SYSNUMS_HEADER1 "syscall/sysnums-arm64.h"
    #define SYSNUMS_ABI1    sysnums_arm64

    #define SYSTRAP_SIZE 4

    #ifndef AUDIT_ARCH_AARCH64
        #define AUDIT_ARCH_AARCH64 (EM_AARCH64 | __AUDIT_ARCH_64BIT | __AUDIT_ARCH_LE)
    #endif

    #define SECCOMP_ARCHS { { .value = AUDIT_ARCH_AARCH64, .nb_abis = 1, .abis = { ABI_DEFAULT } } }

    #define HOST_ELF_MACHINE {183, 0};
    #define RED_ZONE_SIZE 0
    #define OFFSETOF_STAT_UID_32 0
    #define OFFSETOF_STAT_GID_32 0

    #define LOADER_ADDRESS     0x2000000000
    #define EXEC_PIC_ADDRESS   0x3000000000
    #define INTERP_PIC_ADDRESS 0x3f00000000

#elif defined(ARCH_X86)

    #define SYSNUMS_HEADER1 "syscall/sysnums-i386.h"
    #define SYSNUMS_ABI1    sysnums_i386

    #undef  SYSTRAP_NUM
    #define SYSTRAP_NUM SYSARG_RESULT
    #define SYSTRAP_SIZE 2

    #define SECCOMP_ARCHS { { .value = AUDIT_ARCH_I386, .nb_abis = 1, .abis = { ABI_DEFAULT } } }

    #define HOST_ELF_MACHINE {3, 6, 0};
    #define RED_ZONE_SIZE 0
    #define OFFSETOF_STAT_UID_32 0
    #define OFFSETOF_STAT_GID_32 0

    #define LOADER_ADDRESS 0xa0000000
    #define LOADER_ARCH_CFLAGS -mregparm=3

    #define EXEC_PIC_ADDRESS   0x0f000000
    #define INTERP_PIC_ADDRESS 0xaf000000

#elif defined(ARCH_SH4)

    #define SYSNUMS_HEADER1 "syscall/sysnums-sh4.h"
    #define SYSNUMS_ABI1    sysnums_sh4

    #define SYSTRAP_SIZE 2

    #define SECCOMP_ARCHS { }

    #define user_regs_struct pt_regs
    #define HOST_ELF_MACHINE {42, 0};
    #define RED_ZONE_SIZE 0
    #define OFFSETOF_STAT_UID_32 0
    #define OFFSETOF_STAT_GID_32 0
    #define NO_MISALIGNED_ACCESS 1

#else

    #error "Unsupported architecture"

#endif

#endif /* ARCH_H */
