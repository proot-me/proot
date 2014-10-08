/*
  gcc -Isrc -Wall -Wextra -O2						\
  -fno-tree-loop-distribute-patterns -ffreestanding -fPIC  -O3		\
  -static -nostdlib -Wl,-Ttext=0x00007f2000000000			\
  src/loader/loader.c -o src/execve/loader-x86_64
*/

#include <sys/syscall.h> /* SYS_*, */
#include <fcntl.h>       /* O_*, */
#include <sys/mman.h>    /* MAP_*, */
#include <stdbool.h>     /* bool, true, false,  */
#include <linux/auxvec.h>  /* AT_*,  */

typedef unsigned long word_t;
typedef unsigned char byte_t;

#include "loader/script.h"

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
		"                      			\n\t"	\
		"// Clear state flags.			\n\t"	\
		"pushq $0				\n\t"	\
		"popfq					\n\t"	\
		"                      			\n\t"	\
		"// Clear rtld_fini.			\n\t"	\
		"movq $0, %%rdx				\n\t"	\
		"                      			\n\t"	\
		"// Start the program.			\n\t"	\
		"jmpq *%1				\n"	\
		:						\
		: "irm" (stack_pointer), "im" (destination));	\
	__builtin_unreachable();				\
	} while (0)

#define ERROR()	 do {						\
	asm volatile (						\
		"movq $60, %rax		\n\t"			\
		"movq $182, %rdi	\n\t"			\
		"syscall		\n");			\
	__builtin_unreachable ();				\
	} while (0)

#define SC1(result, number, arg1)					\
	asm volatile (							\
		"movq %1, %%rax		\n\t"				\
		"movq %2, %%rdi		\n\t"				\
		"syscall		\n\t"				\
		"movq %%rax, %0		\n"				\
		: "=rm" (result)					\
		: "irm" (number), "irm" (arg1)				\
		: "cc", "rcx", "r11", "rax", "rdi")

#define SC3(result, number, arg1, arg2, arg3)				\
	asm volatile (							\
		"movq %1, %%rax		\n\t"				\
		"movq %2, %%rdi		\n\t"				\
		"movq %3, %%rsi		\n\t"				\
		"movq %4, %%rdx		\n\t"				\
		"syscall		\n\t"				\
		"movq %%rax, %0		\n"				\
		: "=rm" (result)					\
		: "irm" (number), "irm" (arg1), "irm" (arg2), "irm" (arg3) \
		: "cc", "rcx", "r11", "rax", "rdi", "rsi", "rdx")

#define SC6(result, number, arg1, arg2, arg3, arg4, arg5, arg6)		\
	asm volatile (							\
		"movq %1, %%rax		\n\t"				\
		"movq %2, %%rdi		\n\t"				\
		"movq %3, %%rsi		\n\t"				\
		"movq %4, %%rdx		\n\t"				\
		"movq %5, %%r10		\n\t"				\
		"movq %6, %%r8		\n\t"				\
		"movq %7, %%r9		\n\t"				\
		"syscall		\n\t"				\
		"movq %%rax, %0		\n"				\
		: "=rm" (result)					\
		: "irm" (number), "irm" (arg1), "irm" (arg2),		\
		  "irm" (arg3), "irm" (arg4), "irm" (arg5), "irm" (arg6) \
		: "cc", "rcx", "r11", "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9")

#define unlikely(expr) __builtin_expect(!!(expr), 0)

/***********************************************************************/

/**
 * Clear the memory from @start (inclusive) to @end (exclusive).
 */
static inline void clear(word_t start, word_t end)
{
	byte_t *start_misaligned;
	byte_t *end_misaligned;

	word_t *start_aligned;
	word_t *end_aligned;

	/* Compute the number of mis-aligned bytes.  */
	word_t start_bytes = start % sizeof(word_t);
	word_t end_bytes   = end % sizeof(word_t);

	/* Compute aligned addresses.  */
	start_aligned = (word_t *) (start_bytes ? start + sizeof(word_t) - start_bytes : start);
	end_aligned   = (word_t *) (end - end_bytes);

	/* Clear leading mis-aligned bytes.  */
	start_misaligned = (byte_t *) start;
	while (start_misaligned < (byte_t *) start_aligned)
		*start_misaligned++ = 0;

	/* Clear aligned bytes.  */
	while (start_aligned < end_aligned)
		*start_aligned++ = 0;

	/* Clear trailing mis-aligned bytes.  */
	end_misaligned = (byte_t *) end_aligned;
	while (end_misaligned < (byte_t *) end)
		*end_misaligned++ = 0;
}

/**
 * Interpret the load script pointed to by @cursor.
 */
void _start(void *cursor)
{
	bool traced = false;
	bool reset_at_base = true;
	word_t at_base = 0;

	word_t fd = -1;
	word_t status;

	while(1) {
		LoadStatement *stmt = cursor;

		switch (stmt->action) {
		case LOAD_ACTION_OPEN_NEXT:
			SC1(status, SYS_close, fd);
			if (unlikely((int) status < 0))
				ERROR();
			/* Fall through.  */

		case LOAD_ACTION_OPEN:
			SC3(fd, SYS_open, stmt->open.string_address, O_RDONLY, 0);
			if (unlikely((int) fd < 0))
				ERROR();

			reset_at_base = true;

			cursor += LOAD_STATEMENT_SIZE(*stmt, open);
			break;

		case LOAD_ACTION_MMAP_FILE:
			SC6(status, SYS_mmap, stmt->mmap.addr, stmt->mmap.length,
				stmt->mmap.prot, MAP_PRIVATE | MAP_FIXED, fd, stmt->mmap.offset);
			if (unlikely(status != stmt->mmap.addr))
				ERROR();

			if (stmt->mmap.clear_length != 0)
				clear(stmt->mmap.addr + stmt->mmap.length - stmt->mmap.clear_length,
					stmt->mmap.addr + stmt->mmap.length);

			if (reset_at_base) {
				at_base = stmt->mmap.addr;
				reset_at_base = false;
			}

			cursor += LOAD_STATEMENT_SIZE(*stmt, mmap);
			break;

		case LOAD_ACTION_MMAP_ANON:
			SC6(status, SYS_mmap, stmt->mmap.addr, stmt->mmap.length,
				stmt->mmap.prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
			if (unlikely(status != stmt->mmap.addr))
				ERROR();

			cursor += LOAD_STATEMENT_SIZE(*stmt, mmap);
			break;

		case LOAD_ACTION_START_TRACED:
			traced = true;
			/* Fall through.  */

		case LOAD_ACTION_START: {
			word_t *cursor2 = (word_t *) stmt->start.stack_pointer;
			const word_t argc = cursor2[0];
			const word_t at_execfn = cursor2[1];

			SC1(status, SYS_close, fd);
			if (unlikely((int) status < 0))
				ERROR();

			/* Right after execve, the stack content is as follow:
			 *
			 *   +------+--------+--------+--------+
			 *   | argc | argv[] | envp[] | auxv[] |
			 *   +------+--------+--------+--------+
			 */

			/* Skip argv[].  */
			cursor2 += argc + 1;

			/* Skip envp[].  */
			do cursor2++; while (cursor2[0] != 0);
			cursor2++;

			/* Adjust auxv[].  */
			do {
				switch (cursor2[0]) {
				case AT_PHDR:
					cursor2[1] = stmt->start.at_phdr;
					break;

				case AT_PHENT:
					cursor2[1] = stmt->start.at_phent;
					break;

				case AT_PHNUM:
					cursor2[1] = stmt->start.at_phnum;
					break;

				case AT_ENTRY:
					cursor2[1] = stmt->start.at_entry;
					break;

				case AT_BASE:
					cursor2[1] = at_base;
					break;

				case AT_EXECFN:
					cursor2[1] = at_execfn;
					break;

				default:
					break;
				}
				cursor2 += 2;
			} while (cursor2[0] != AT_NULL);

			if (unlikely(traced))
				SC6(status, SYS_execve, -1,
					stmt->start.stack_pointer,
					stmt->start.entry_point, -2, -3, -4);
			else
				BRANCH(stmt->start.stack_pointer, stmt->start.entry_point);
			ERROR();
		}

		default:
			ERROR();
		}
	}

	ERROR();
}
