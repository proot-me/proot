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
 */

#ifndef ARCH_H
#define ARCH_H

typedef unsigned long word_t;

/* Specify the ABI registers (syscall argument passing, stack pointer). */
#if defined(x86_64)
	#define REG_SYSARG_NUM	orig_rax
	#define REG_SYSARG_1	rdi
	#define REG_SYSARG_2	rsi
	#define REG_SYSARG_3	rdx
	#define REG_SYSARG_4	r10
	#define REG_SYSARG_5	r9
	#define REG_SYSARG_6	r8
	#define REG_SYSARG_RESULT	rax
	#define REG_SP		rsp
#elif defined(i386) || defined(i486) || defined(i586) || defined(i686)
	#warning "Untested architecture"
	#define REG_SYSARG_NUM	orig_eax
	#define REG_SYSARG_1	ebx
	#define REG_SYSARG_2	ecx
	#define REG_SYSARG_3	edx
	#define REG_SYSARG_4	esi
	#define REG_SYSARG_5	edi
	#define REG_SYSARG_6	ebp
	#define REG_SYSARG_RESULT	eax
	#define REG_SP		esp
#elif defined(sh4)
	#warning "Untested architecture"
	#define REG_SYSARG_NUM	r4
	#define REG_SYSARG_1	r5
	#define REG_SYSARG_2	r6
	#define REG_SYSARG_3	r7
	#define REG_SYSARG_4	r0
	#define REG_SYSARG_5	r1
	#define REG_SYSARG_6	r2
	#define REG_SYSARG_RESULT	r0
	#define REG_SP		r15
#else
	#error "Unsupported architecture"
#endif

#endif /* ARCH_H */
