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

/* According to the x86 ABI, all registers have undefined values at
 * program startup except:
 *
 * - the instruction pointer (rip)
 * - the stack pointer (rsp)
 * - the rtld_fini pointer (rdx)
 * - the system flags (eflags)
 */
#define BRANCH(stack_pointer, destination) do {			\
	asm volatile (						\
		"// Restore initial stack pointer.	\n\t"	\
		"movl %0, %%esp				\n\t"	\
		"                      			\n\t"	\
		"// Clear state flags.			\n\t"	\
		"pushl $0				\n\t"	\
		"popfl					\n\t"	\
		"                      			\n\t"	\
		"// Clear rtld_fini.			\n\t"	\
		"movl $0, %%edx				\n\t"	\
		"                      			\n\t"	\
		"// Start the program.			\n\t"	\
		"jmpl *%%eax				\n"	\
		: /* no output */				\
		: "irm" (stack_pointer), "a" (destination)	\
		: "memory", "cc", "esp", "edx");		\
	__builtin_unreachable();				\
	} while (0)

extern word_t syscall_6(word_t number,
			word_t arg1, word_t arg2, word_t arg3,
			word_t arg4, word_t arg5, word_t arg6);

extern word_t syscall_3(word_t number, word_t arg1, word_t arg2, word_t arg3);

extern word_t syscall_1(word_t number, word_t arg1);

#define SYSCALL(number, nb_args, args...) syscall_##nb_args(number, args)

#define OPEN	5
#define CLOSE	6
#define MMAP	192
#define MMAP_OFFSET_SHIFT 12
#define EXECVE	11
#define EXIT	1
#define PRCTL	172
#define MPROTECT 125
