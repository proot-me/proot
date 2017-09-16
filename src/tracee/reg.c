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

#include <sys/types.h>  /* off_t */
#include <sys/user.h>   /* struct user*, */
#include <sys/ptrace.h> /* ptrace(2), PTRACE*, */
#include <assert.h>     /* assert(3), */
#include <errno.h>      /* errno(3), */
#include <stddef.h>     /* offsetof(), */
#include <stdint.h>     /* *int*_t, */
#include <inttypes.h>   /* PRI*, */
#include <limits.h>     /* ULONG_MAX, */
#include <string.h>     /* memcpy(3), */
#include <sys/uio.h>    /* struct iovec, */

#include "arch.h"

#if defined(ARCH_ARM64)
#include <linux/elf.h>  /* NT_PRSTATUS */
#endif

#include "syscall/sysnum.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "cli/note.h"
#include "compat.h"

/**
 * Compute the offset of the register @reg_name in the USER area.
 */
#define USER_REGS_OFFSET(reg_name)			\
	(offsetof(struct user, regs)			\
	 + offsetof(struct user_regs_struct, reg_name))

#define REG(tracee, version, index)			\
	(*(word_t*) (((uint8_t *) &tracee->_regs[version]) + reg_offset[index]))

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
	[INSTR_POINTER] = USER_REGS_OFFSET(rip),
	[RTLD_FINI]     = USER_REGS_OFFSET(rdx),
	[STATE_FLAGS]   = USER_REGS_OFFSET(eflags),
	[USERARG_1]     = USER_REGS_OFFSET(rdi),
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
	[INSTR_POINTER] = USER_REGS_OFFSET(rip),
	[RTLD_FINI]     = USER_REGS_OFFSET(rdx),
	[STATE_FLAGS]   = USER_REGS_OFFSET(eflags),
	[USERARG_1]     = USER_REGS_OFFSET(rax),
    };

    #undef  REG
    #define REG(tracee, version, index)					\
	(*(word_t*) (tracee->_regs[version].cs == 0x23			\
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
	[INSTR_POINTER] = USER_REGS_OFFSET(uregs[15]),
	[USERARG_1]     = USER_REGS_OFFSET(uregs[0]),
    };

#elif defined(ARCH_ARM64)

    #undef  USER_REGS_OFFSET
    #define USER_REGS_OFFSET(reg_name) offsetof(struct user_regs_struct, reg_name)

    static off_t reg_offset[] = {
	[SYSARG_NUM]    = USER_REGS_OFFSET(regs[8]),
	[SYSARG_1]      = USER_REGS_OFFSET(regs[0]),
	[SYSARG_2]      = USER_REGS_OFFSET(regs[1]),
	[SYSARG_3]      = USER_REGS_OFFSET(regs[2]),
	[SYSARG_4]      = USER_REGS_OFFSET(regs[3]),
	[SYSARG_5]      = USER_REGS_OFFSET(regs[4]),
	[SYSARG_6]      = USER_REGS_OFFSET(regs[5]),
	[SYSARG_RESULT] = USER_REGS_OFFSET(regs[0]),
	[STACK_POINTER] = USER_REGS_OFFSET(sp),
	[INSTR_POINTER] = USER_REGS_OFFSET(pc),
	[USERARG_1]     = USER_REGS_OFFSET(regs[0]),
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
	[INSTR_POINTER] = USER_REGS_OFFSET(eip),
	[RTLD_FINI]     = USER_REGS_OFFSET(edx),
	[STATE_FLAGS]   = USER_REGS_OFFSET(eflags),
	[USERARG_1]     = USER_REGS_OFFSET(eax),
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
	[INSTR_POINTER] = USER_REGS_OFFSET(pc),
	[RTLD_FINI]     = USER_REGS_OFFSET(r4),
    };

#else

    #error "Unsupported architecture"

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
	tracee->_regs_were_changed = true;
}

/**
 * Print the value of the current @tracee's registers according
 * to the @verbose_level.  Note: @message is mixed to the output.
 */
void print_current_regs(Tracee *tracee, int verbose_level, const char *message)
{
	if (tracee->verbose < verbose_level)
		return;

	note(tracee, INFO, INTERNAL,
		"vpid %" PRIu64 ": %s: %s(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) = 0x%lx [0x%lx, %d]",
		tracee->vpid, message,
		stringify_sysnum(get_sysnum(tracee, CURRENT)),
		peek_reg(tracee, CURRENT, SYSARG_1), peek_reg(tracee, CURRENT, SYSARG_2),
		peek_reg(tracee, CURRENT, SYSARG_3), peek_reg(tracee, CURRENT, SYSARG_4),
		peek_reg(tracee, CURRENT, SYSARG_5), peek_reg(tracee, CURRENT, SYSARG_6),
		peek_reg(tracee, CURRENT, SYSARG_RESULT),
		peek_reg(tracee, CURRENT, STACK_POINTER),
		get_abi(tracee));
}

/**
 * Save the @tracee's current register bank into the @version register
 * bank.
 */
void save_current_regs(Tracee *tracee, RegVersion version)
{
	/* Optimization: don't restore original register values if
	 * they were never changed.  */
	if (version == ORIGINAL)
		tracee->_regs_were_changed = false;

	memcpy(&tracee->_regs[version], &tracee->_regs[CURRENT], sizeof(tracee->_regs[CURRENT]));
}

/**
 * Copy all @tracee's general purpose registers into a dedicated
 * cache.  This function returns -errno if an error occured, 0
 * otherwise.
 */
int fetch_regs(Tracee *tracee)
{
	int status;

#if defined(ARCH_ARM64)
	struct iovec regs;

	regs.iov_base = &tracee->_regs[CURRENT];
	regs.iov_len  = sizeof(tracee->_regs[CURRENT]);

	status = ptrace(PTRACE_GETREGSET, tracee->pid, NT_PRSTATUS, &regs);
#else
	status = ptrace(PTRACE_GETREGS, tracee->pid, NULL, &tracee->_regs[CURRENT]);
#endif
	if (status < 0)
		return status;

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

	if (tracee->_regs_were_changed) {
		/* At the very end of a syscall, with regard to the
		 * entry, only the result register can be modified by
		 * PRoot.  */
		if (tracee->restore_original_regs) {
			/* Restore the sysarg register only if it is
			 * not the same as the result register.  Note:
			 * it's never the case on x86 architectures,
			 * so don't make this check, otherwise it
			 * would introduce useless complexity because
			 * of the multiple ABI support.  */
#if defined(ARCH_X86) || defined(ARCH_X86_64)
#    define		RESTORE(sysarg)	(REG(tracee, CURRENT, sysarg) = REG(tracee, ORIGINAL, sysarg))
#else
#    define	 	RESTORE(sysarg) (void) (reg_offset[SYSARG_RESULT] != reg_offset[sysarg] && \
				(REG(tracee, CURRENT, sysarg) = REG(tracee, ORIGINAL, sysarg)))
#endif

			RESTORE(SYSARG_NUM);
			RESTORE(SYSARG_1);
			RESTORE(SYSARG_2);
			RESTORE(SYSARG_3);
			RESTORE(SYSARG_4);
			RESTORE(SYSARG_5);
			RESTORE(SYSARG_6);
			RESTORE(STACK_POINTER);
		}

#if defined(ARCH_ARM64)
		struct iovec regs;
		word_t current_sysnum = REG(tracee, CURRENT, SYSARG_NUM);

		/* Update syscall number if needed.  On arm64, a new
		 * subcommand has been added to PTRACE_{S,G}ETREGSET
		 * to allow write/read of current sycall number.  */
		if (current_sysnum != REG(tracee, ORIGINAL, SYSARG_NUM)) {
			regs.iov_base = &current_sysnum;
			regs.iov_len = sizeof(current_sysnum);
			status = ptrace(PTRACE_SETREGSET, tracee->pid, NT_ARM_SYSTEM_CALL, &regs);
			if (status < 0)
				note(tracee, WARNING, SYSTEM, "can't set the syscall number");
		}

		/* Update other registers.  */
		regs.iov_base = &tracee->_regs[CURRENT];
		regs.iov_len  = sizeof(tracee->_regs[CURRENT]);

		status = ptrace(PTRACE_SETREGSET, tracee->pid, NT_PRSTATUS, &regs);
#else
#    if defined(ARCH_ARM_EABI)
		/* On ARM, a special ptrace request is required to
		 * change effectively the syscall number during a
		 * ptrace-stop.  */
		word_t current_sysnum = REG(tracee, CURRENT, SYSARG_NUM);
		if (current_sysnum != REG(tracee, ORIGINAL, SYSARG_NUM)) {
			status = ptrace(PTRACE_SET_SYSCALL, tracee->pid, 0, current_sysnum);
			if (status < 0)
				note(tracee, WARNING, SYSTEM, "can't set the syscall number");
		}
#    endif

		status = ptrace(PTRACE_SETREGS, tracee->pid, NULL, &tracee->_regs[CURRENT]);
#endif
		if (status < 0)
			return status;
	}

	return 0;
}
