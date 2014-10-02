/* gcc -Isrc -fPIC -nostartfiles -Wall -Wextra src/loader/loader.c -O3 -o src/loader/loader -g -static -Wl,-Ttext=0x00007f2000000000 */

#include <sys/syscall.h> /* SYS_*, */
#include <fcntl.h>       /* O_*, */
#include <sys/mman.h>    /* MAP_*, */
#include <strings.h>     /* bzero(3), */

typedef unsigned long word_t;

#include "loader/script.h"

/* TODO: set all registers to 0.  */
#define BRANCH(stack_pointer, destination)			\
	asm volatile (						\
		"movq %0, %%rsp		\n\t"			\
		"jmpq *%1		\n"			\
		: : "irm" (stack_pointer), "irm" (destination) )

#define ERROR()							\
	asm volatile (						\
		"movq $60, %rax		\n\t"			\
		"movq $182, %rdi	\n\t"			\
		"syscall		\n")

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

/* Note: this is optimized both for speed and for memory
 * footprint.  */
void _start(void *cursor)
{
	word_t status;
	word_t fd = -1;

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

			cursor += LOAD_STATEMENT_SIZE(*stmt, open);
			break;

		case LOAD_ACTION_MMAP_FILE:
			SC6(status, SYS_mmap, stmt->mmap.addr, stmt->mmap.length,
				stmt->mmap.prot, MAP_PRIVATE | MAP_FIXED, fd, stmt->mmap.offset);
			if (unlikely(status != stmt->mmap.addr))
				ERROR();

			if (stmt->mmap.clear_length != 0) {
				bzero((void *) (stmt->mmap.addr
						+ stmt->mmap.length
						- stmt->mmap.clear_length),
					stmt->mmap.clear_length);
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

		case LOAD_ACTION_START:
			SC1(status, SYS_close, fd);
			if (unlikely((int) status < 0))
				ERROR();

			BRANCH(stmt->start.stack_pointer, stmt->start.entry_point);
			/* Fall through.  */

		default:
			ERROR(); /* Never reached.  */
		}
	}

	ERROR(); /* Never reached.  */
}
