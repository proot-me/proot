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

#include <sys/types.h>  /* off_t */
#include <sys/user.h>   /* struct user*, */
#include <sys/ptrace.h> /* ptrace(2), PTRACE*, */
#include <assert.h>     /* assert(3), */
#include <errno.h>      /* errno(3), */

#include "tracee/ureg.h"
#include "notice.h"  /* notice(), */
#include "syscall/syscall.h" /* enum sysarg, STACK_POINTER, */

/* Specify the ABI registers (syscall argument passing, stack pointer).
 * See sysdeps/unix/sysv/linux/${ARCH}/syscall.S from the GNU C Library. */
#if defined(ARCH_X86_64)

    off_t uregs[UREGS_LENGTH] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rdi),
	[SYSARG_2]      = USER_REGS_OFFSET(rsi),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(r10),
	[SYSARG_5]      = USER_REGS_OFFSET(r9),
	[SYSARG_6]      = USER_REGS_OFFSET(r8),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp),
    };

    off_t uregs2[UREGS_LENGTH] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rbx),
	[SYSARG_2]      = USER_REGS_OFFSET(rcx),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(rsi),
	[SYSARG_5]      = USER_REGS_OFFSET(rdi),
	[SYSARG_6]      = USER_REGS_OFFSET(rbp),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp)
    };

#elif defined(ARCH_ARM_EABI)

    off_t uregs[UREGS_LENGTH] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(uregs[7]),
	[SYSARG_1]      = USER_REGS_OFFSET(uregs[0]),
	[SYSARG_2]      = USER_REGS_OFFSET(uregs[1]),
	[SYSARG_3]      = USER_REGS_OFFSET(uregs[2]),
	[SYSARG_4]      = USER_REGS_OFFSET(uregs[3]),
	[SYSARG_5]      = USER_REGS_OFFSET(uregs[4]),
	[SYSARG_6]      = USER_REGS_OFFSET(uregs[5]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(uregs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(uregs[13]),
    };

#elif defined(ARCH_X86)

    off_t uregs[UREGS_LENGTH] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_eax),
	[SYSARG_1]      = USER_REGS_OFFSET(ebx),
	[SYSARG_2]      = USER_REGS_OFFSET(ecx),
	[SYSARG_3]      = USER_REGS_OFFSET(edx),
	[SYSARG_4]      = USER_REGS_OFFSET(esi),
	[SYSARG_5]      = USER_REGS_OFFSET(edi),
	[SYSARG_6]      = USER_REGS_OFFSET(ebp),
	[SYSARG_RESULT] = USER_REGS_OFFSET(eax),
	[STACK_POINTER] = USER_REGS_OFFSET(esp),
    };

#elif defined(ARCH_SH4)

    off_t uregs[UREGS_LENGTH] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(regs[3]),
	[SYSARG_1]      = USER_REGS_OFFSET(regs[4]),
	[SYSARG_2]      = USER_REGS_OFFSET(regs[5]),
	[SYSARG_3]      = USER_REGS_OFFSET(regs[6]),
	[SYSARG_4]      = USER_REGS_OFFSET(regs[7]),
	[SYSARG_5]      = USER_REGS_OFFSET(regs[0]),
	[SYSARG_6]      = USER_REGS_OFFSET(regs[1]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(regs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(regs[15]),
    };

#else

    #error "Unsupported architecture"

#endif

/**
 * Return the value of the @tracee's register @index (regarding
 * uregs[]). This function sets the special global variable errno if
 * an error occured.
 */
word_t peek_ureg(struct tracee_info *tracee, int index)
{
	word_t result;

	/* Sanity checks. */
	assert(index >= SYSARG_NUM);
	assert(index <= STACK_POINTER);

	/* Get the argument register from the tracee's USER area. */
	result = ptrace(PTRACE_PEEKUSER, tracee->pid, tracee->uregs[index], NULL);

#ifdef ARCH_X86_64
	/* Use only the 32 least significant bits (LSB) when running
	 * 32-bit processes on x86_64. */
	if (tracee->uregs == uregs2)
		result &= 0xFFFFFFFF;
#endif

	return result;
}

/**
 * Set the @tracee's register @index (regarding uregs[]) to @value.
 * This function returns -errno if an error occured.
 */
int poke_ureg(struct tracee_info *tracee, int index, word_t value)
{
	  long status;

	  /* Sanity checks. */
	  assert(index >= SYSARG_NUM);
	  assert(index <= STACK_POINTER);

#ifdef ARCH_X86_64
	/* Check we are using only the 32 LSB when running 32-bit
	 * processes on x86_64. */
	if (tracee->uregs == uregs2
	    && (value >> 32) != 0
	    && (value >> 32) != 0xFFFFFFFF)
		notice(WARNING, INTERNAL, "value too large for a 32-bit register");
#endif

	  /* Set the argument register in the tracee's USER area. */
	  status = ptrace(PTRACE_POKEUSER, tracee->pid, tracee->uregs[index], value);
	  if (status < 0)
		  return -errno;

	  return 0;
}
