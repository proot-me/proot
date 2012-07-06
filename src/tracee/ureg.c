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
#include <stddef.h>     /* offsetof(), */
#include <stdint.h>     /* *int*_t(), */

#include "tracee/ureg.h"
#include "notice.h"  /* notice(), */

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

/* Specify the ABI registers (syscall argument passing, stack pointer).
 * See sysdeps/unix/sysv/linux/${ARCH}/syscall.S from the GNU C Library. */
#if defined(ARCH_X86_64)

    static off_t ureg_offset[] = {
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

    static off_t ureg_offset_x86[] = {
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

    #define UREG(tracee, index)							\
	(*(word_t*) (tracee->_uregs.cache.cs == 0x23				\
	     ? (((uint8_t *) &tracee->_uregs.cache) + ureg_offset_x86[index])	\
	     : (((uint8_t *) &tracee->_uregs.cache) + ureg_offset[index])))

#elif defined(ARCH_ARM_EABI)

    static off_t ureg_offset[] = {
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

    static off_t ureg_offset[] = {
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

    static off_t ureg_offset[] = {
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

#if !defined(UREG)
    #define UREG(tracee, index) \
	(*(word_t*) (((uint8_t *) &tracee->_uregs.cache) + ureg_offset[index]))
#endif

/**
 * Return the *cached* value of the given @tracees' @ureg.
 */
word_t peek_ureg(const struct tracee_info *tracee, enum ureg ureg)
{
	word_t result;

	/* Sanity checks. */
	assert(ureg >= UREG_FIRST);
	assert(ureg <= UREG_LAST);
	assert(   tracee->_uregs.state == UREGS_ARE_VALID
	       || tracee->_uregs.state == UREGS_HAVE_CHANGED);

	result = UREG(tracee, ureg);

#if ARCH_X86_64
	/* Use only the 32 least significant bits (LSB) when running
	 * 32-bit processes on x86_64. */
	if (get_abi(tracee) == ABI_X86)
		result &= 0xFFFFFFFF;
#endif

	return result;
}

/**
 * Set the *cached* value of the given @tracees' @ureg.
 */
void poke_ureg(struct tracee_info *tracee, enum ureg ureg, word_t value)
{
	/* Sanity checks. */
	assert(ureg >= UREG_FIRST);
	assert(ureg <= UREG_LAST);
	assert(   tracee->_uregs.state == UREGS_ARE_VALID
	       || tracee->_uregs.state == UREGS_HAVE_CHANGED);

#if defined(ARCH_X86_64)
	/* Check we are using only the 32 LSB when running 32-bit
	 * processes on x86_64. */
	if (get_abi(tracee) == ABI_X86
	    && (value >> 32) != 0
	    && (value >> 32) != 0xFFFFFFFF)
		notice(WARNING, INTERNAL, "value too large for a 32-bit register");
#endif

	UREG(tracee, ureg) = value;
	tracee->_uregs.state = UREGS_HAVE_CHANGED;
}

/**
 * Copy all @tracee's general purpose registers into a dedicated
 * cache.  This function returns -errno if an error occured, 0
 * otherwise.
 */
int fetch_uregs(struct tracee_info *tracee)
{
	int status;

	assert(tracee->_uregs.state == UREGS_ARE_INVALID);

	status = ptrace(PTRACE_GETREGS, tracee->pid, NULL, &tracee->_uregs.cache);
	if (status < 0)
		return status;

	tracee->_uregs.state = UREGS_ARE_VALID;
	return 0;
}

/**
 * Copy the cached values of all @tracee's general purpose registers
 * back to the process, if necessary.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
int push_uregs(struct tracee_info *tracee)
{
	int status;

	switch(tracee->_uregs.state) {
	case UREGS_ARE_INVALID:
		assert(0);
		break;

	case UREGS_ARE_VALID:
		/* Nothing to do.  */
		break;

	case UREGS_HAVE_CHANGED:
		status = ptrace(PTRACE_SETREGS, tracee->pid, NULL, &tracee->_uregs.cache);
		if (status < 0)
			return status;
		break;
	}

	tracee->_uregs.state = UREGS_ARE_INVALID;
	return 0;
}

/**
 * Return the ABI currently used by the given @tracee.
 */
enum abi get_abi(const struct tracee_info *tracee)
{
	/* Sanity checks. */
	assert(   tracee->_uregs.state == UREGS_ARE_VALID
	       || tracee->_uregs.state == UREGS_HAVE_CHANGED);

#if defined(ARCH_X86_64)
	return (tracee->_uregs.cache.cs == 0x23 ? ABI_X86 : ABI_DEFAULT);
#else
	return ABI_DEFAULT;
#endif /* ARCH_X86_64 */
}
