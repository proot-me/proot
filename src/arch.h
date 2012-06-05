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

#ifndef ARCH_H
#define ARCH_H

typedef unsigned long word_t;

#define SYSCALL_AVOIDER -2

#if !defined(ARCH_X86_64) && !defined(ARCH_ARM_EABI) && !defined(ARCH_X86) && !defined(ARCH_SH4)
#    if defined(__x86_64__)
#        define ARCH_X86_64 1
#    elif defined(__ARM_EABI__)
#        define ARCH_ARM_EABI 1
#        warning "Untested architecture"
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

    #define SYSNUM_HEADER  "syscall/sysnum-x86_64.h"
    #define SYSNUM_HEADER2 "syscall/sysnum-i386.h"
    #define HOST_ELF_MACHINE {62, 3, 6, 0}

#elif defined(ARCH_ARM_EABI)

    #define user_regs_struct user_regs
    #define SYSNUM_HEADER "syscall/sysnum-arm.h"
    #define HOST_ELF_MACHINE {40, 0};

#elif defined(ARCH_X86)

    #define SYSNUM_HEADER "syscall/sysnum-i386.h"
    #define HOST_ELF_MACHINE {3, 6, 0};

#elif defined(ARCH_SH4)

    #define user_regs_struct pt_regs
    #define SYSNUM_HEADER "syscall/sysnum-sh4.h"
    #define HOST_ELF_MACHINE {42, 0};
    #define NO_MISALIGNED_ACCESS 1
#else

    #error "Unsupported architecture"

#endif

#endif /* ARCH_H */
