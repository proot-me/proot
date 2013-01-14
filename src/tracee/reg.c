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

#include <sys/types.h>  /* off_t */
#include <sys/user.h>   /* struct user*, */
#include <sys/ptrace.h> /* ptrace(2), PTRACE*, */
#include <assert.h>     /* assert(3), */
#include <errno.h>      /* errno(3), */
#include <stddef.h>     /* offsetof(), */
#include <stdint.h>     /* *int*_t(), */
#include <limits.h>     /* ULONG_MAX, */
#include <string.h>     /* memcpy(3), */

#include "tracee/reg.h"
#include "tracee/abi.h"

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

/* Specify the ABI registers (syscall argument passing, stack pointer).
 * See sysdeps/unix/sysv/linux/${ARCH}/syscall.S from the GNU C Library. */
#if defined(ARCH_X86_64)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rdi),
	[SYSARG_2]      = USER_REGS_OFFSET(rsi),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(r10),
	[SYSARG_5]      = USER_REGS_OFFSET(r8),
	[SYSARG_6]      = USER_REGS_OFFSET(r9),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp),
    };

    static off_t reg_offset_x86[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(orig_rax),
	[SYSARG_1]      = USER_REGS_OFFSET(rbx),
	[SYSARG_2]      = USER_REGS_OFFSET(rcx),
	[SYSARG_3]      = USER_REGS_OFFSET(rdx),
	[SYSARG_4]      = USER_REGS_OFFSET(rsi),
	[SYSARG_5]      = USER_REGS_OFFSET(rdi),
	[SYSARG_6]      = USER_REGS_OFFSET(rbp),
	[SYSARG_RESULT] = USER_REGS_OFFSET(rax),
	[STACK_POINTER] = USER_REGS_OFFSET(rsp),
    };

    #define REG(tracee, version, index)					\
	(*(word_t*) (tracee->_regs[ORIGINAL].cs == 0x23			\
		? (((uint8_t *) &tracee->_regs[version]) + reg_offset_x86[index]) \
		: (((uint8_t *) &tracee->_regs[version]) + reg_offset[index])))

#elif defined(ARCH_ARM_EABI)

    static off_t reg_offset[] = {
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

    static off_t reg_offset[] = {
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

    static off_t reg_offset[] = {
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

#if !defined(REG)
    #define REG(tracee, version, index)					\
	(*(word_t*) (((uint8_t *) &tracee->_regs[version]) + reg_offset[index]))
#endif

/**
 * Return the *cached* value of the given @tracees' @reg.
 */
word_t peek_reg(const Tracee *tracee, RegVersion version, Reg reg)
{
	word_t result;

	assert(version < NB_REG_VERSION);

	result = REG(tracee, version, reg);

	/* Use only the 32 least significant bits (LSB) when running
	 * 32-bit processes on a 64-bit kernel.  */
	if (is_32on64_mode(tracee))
		result &= 0xFFFFFFFF;

	return result;
}

/**
 * Set the *cached* value of the given @tracees' @reg.
 */
void poke_reg(Tracee *tracee, Reg reg, word_t value)
{
	if (peek_reg(tracee, CURRENT, reg) == value)
		return;

	REG(tracee, CURRENT, reg) = value;
	tracee->_regs_have_changed = true;
}

/**
 * Copy all @tracee's general purpose registers into a dedicated
 * cache.  This function returns -errno if an error occured, 0
 * otherwise.
 */
int fetch_regs(Tracee *tracee)
{
	int status;

	status = ptrace(PTRACE_GETREGS, tracee->pid, NULL, &tracee->_regs[CURRENT]);
	if (status < 0)
		return status;
	tracee->_regs_have_changed = false;

	if (tracee->status == 0)
		memcpy(&tracee->_regs[ORIGINAL], &tracee->_regs[CURRENT], sizeof(struct user_regs_struct));

	return 0;
}

/**
 * Copy the cached values of all @tracee's general purpose registers
 * back to the process, if necessary.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
int push_regs(Tracee *tracee)
{
	int status;

	if (tracee->_regs_have_changed) {
		status = ptrace(PTRACE_SETREGS, tracee->pid, NULL, &tracee->_regs[CURRENT]);
		if (status < 0)
			return status;
	}

	if (tracee->status != 0)
		memcpy(&tracee->_regs[MODIFIED], &tracee->_regs[CURRENT], sizeof(struct user_regs_struct));

	return 0;
}
