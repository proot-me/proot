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
		"jmpq *%%rax				\n"	\
		: /* no output */				\
		: "irm" (stack_pointer), "a" (destination)	\
		: "cc", "rsp", "rdx");				\
	__builtin_unreachable();				\
	} while (0)

#define PREPARE_ARGS_1(arg1_)						\
	register word_t arg1 asm("rdi") = arg1_;			\

#define PREPARE_ARGS_3(arg1_, arg2_, arg3_)				\
	PREPARE_ARGS_1(arg1_)						\
	register word_t arg2 asm("rsi") = arg2_;			\
	register word_t arg3 asm("rdx") = arg3_;			\

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
		asm volatile (						\
			"syscall		\n\t"			\
			: "=r" (result)					\
			: "r" (number),					\
			  OUTPUT_CONTRAINTS_##nb_args			\
			: "cc", "rcx", "r11");				\
		result;							\
	})

/***********************************************************************/

#define FATAL() do {						\
		SYSCALL(SYS_exit, 1, 182);			\
		__builtin_unreachable ();			\
	} while (0)

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
			status = SYSCALL(SYS_close, 1, fd);
			if (unlikely((int) status < 0))
				FATAL();
			/* Fall through.  */

		case LOAD_ACTION_OPEN:
			fd = SYSCALL(SYS_open, 3, stmt->open.string_address, O_RDONLY, 0);
			if (unlikely((int) fd < 0))
				FATAL();

			reset_at_base = true;

			cursor += LOAD_STATEMENT_SIZE(*stmt, open);
			break;

		case LOAD_ACTION_MMAP_FILE:
			status = SYSCALL(SYS_mmap, 6, stmt->mmap.addr, stmt->mmap.length,
					stmt->mmap.prot, MAP_PRIVATE | MAP_FIXED, fd, stmt->mmap.offset);
			if (unlikely(status != stmt->mmap.addr))
				FATAL();

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
			status = SYSCALL(SYS_mmap, 6, stmt->mmap.addr, stmt->mmap.length,
					stmt->mmap.prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
			if (unlikely(status != stmt->mmap.addr))
				FATAL();

			cursor += LOAD_STATEMENT_SIZE(*stmt, mmap);
			break;

		case LOAD_ACTION_START_TRACED:
			traced = true;
			/* Fall through.  */

		case LOAD_ACTION_START: {
			word_t *cursor2 = (word_t *) stmt->start.stack_pointer;
			const word_t argc = cursor2[0];
			const word_t at_execfn = cursor2[1];

			status = SYSCALL(SYS_close, 1, fd);
			if (unlikely((int) status < 0))
				FATAL();

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
				SYSCALL(SYS_execve, 6, -1,
					stmt->start.stack_pointer,
					stmt->start.entry_point, -2, -3, -4);
			else
				BRANCH(stmt->start.stack_pointer, stmt->start.entry_point);
			FATAL();
		}

		default:
			FATAL();
		}
	}

	FATAL();
}
