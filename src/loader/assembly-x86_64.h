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

/* According to the x86_64 ABI, all registers have undefined values at
 * program startup except:
 *
 * - the instruction pointer (rip)
 * - the stack pointer (rsp)
 * - the rtld_fini pointer (rdx)
 * - the system flags (rflags)
 */
#define BRANCH(stack_pointer, destination) do {			\
	asm volatile (						\
		"// Restore initial stack pointer.	\n\t"	\
		"movq %0, %%rsp				\n\t"	\
		"					\n\t"	\
		"// Clear state flags.			\n\t"	\
		"pushq $0				\n\t"	\
		"popfq					\n\t"	\
		"					\n\t"	\
		"// Clear rtld_fini.			\n\t"	\
		"movq $0, %%rdx				\n\t"	\
		"					\n\t"	\
		"// Start the program.			\n\t"	\
		"jmpq *%%rax				\n"	\
		: /* no output */				\
		: "irm" (stack_pointer), "a" (destination)	\
		: "memory", "cc", "rsp", "rdx");		\
	__builtin_unreachable();				\
	} while (0)

#define PREPARE_ARGS_1(arg1_)				\
	register word_t arg1 asm("rdi") = arg1_;	\

#define PREPARE_ARGS_3(arg1_, arg2_, arg3_)		\
	PREPARE_ARGS_1(arg1_)				\
	register word_t arg2 asm("rsi") = arg2_;	\
	register word_t arg3 asm("rdx") = arg3_;	\

#define PREPARE_ARGS_6(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_)	\
	PREPARE_ARGS_3(arg1_, arg2_, arg3_)				\
	register word_t arg4 asm("r10") = arg4_;			\
	register word_t arg5 asm("r8")  = arg5_;			\
	register word_t arg6 asm("r9")  = arg6_;

#define OUTPUT_CONTRAINTS_1			\
	"r" (arg1)

#define OUTPUT_CONTRAINTS_3			\
	OUTPUT_CONTRAINTS_1,			\
	"r" (arg2), "r" (arg3)

#define OUTPUT_CONTRAINTS_6			\
	OUTPUT_CONTRAINTS_3,			\
	"r" (arg4), "r" (arg5), "r" (arg6)

#define SYSCALL(number_, nb_args, args...)				\
	({								\
		register word_t number asm("rax") = number_;		\
		register word_t result asm("rax");			\
		PREPARE_ARGS_##nb_args(args)				\
			asm volatile (					\
				"syscall		\n\t"		\
				: "=r" (result)				\
				: "r" (number),				\
				OUTPUT_CONTRAINTS_##nb_args		\
				: "memory", "cc", "rcx", "r11");	\
			result;						\
	})

#define OPEN	2
#define CLOSE	3
#define MMAP	9
#define EXECVE	59
#define EXIT	60
#define PRCTL	157
#define MPROTECT 10
