#include <sys/mman.h>	/* PROT_*, MAP_*, */
#include <assert.h>	/* assert(3),  */
#include <string.h>     /* strerror(3), */

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "syscall/sysnum.h"
#include "cli/notice.h"

#include "compat.h"

#define DEBUG_BRK(...) /* fprintf(stderr, __VA_ARGS__) */

/* The size of the heap can be zero, unlike the size of a memory
 * mapping.  As a consequence, the first page of the "heap" memory
 * mapping is discarded in order to emulate an empty heap.  */
#define HEAP_OFFSET 0x1000

word_t translate_brk_enter(Tracee *tracee)
{
	word_t address;

	address = peek_reg(tracee, CURRENT, SYSARG_1);
	DEBUG_BRK("brk(0x%lx)\n", address);

	/* Allocate a new mapping for the emulated heap.  */
	if (tracee->heap->base == 0) {
		Sysnum sysnum;

		if (address != 0)
			notice(tracee, WARNING, INTERNAL,
				"process %d is doing suspicious brk()",	tracee->pid);

		/* I don't understand yet why mmap(2) fails (EFAULT)
		 * on architectures that also have mmap2(2).  Maybe
		 * this former implies MAP_FIXED in such cases.  */
		sysnum = detranslate_sysnum(get_abi(tracee), PR_mmap2) != SYSCALL_AVOIDER
			? PR_mmap2
			: PR_mmap;

		set_sysnum(tracee, sysnum);
		poke_reg(tracee, SYSARG_1 /* address */, 0);
		poke_reg(tracee, SYSARG_2 /* length  */, HEAP_OFFSET);
		poke_reg(tracee, SYSARG_3 /* prot    */, PROT_READ | PROT_WRITE);
		poke_reg(tracee, SYSARG_4 /* flags   */, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT);
		poke_reg(tracee, SYSARG_5 /* fd      */, -1);
		poke_reg(tracee, SYSARG_6 /* offset  */, 0);

		return 0;
	}

	/* The size if the heap can't be negative.  */
	if (address < tracee->heap->base) {
		set_sysnum(tracee, PR_void);
		return 0;
	}

	/* Resize the heap.  */
	set_sysnum(tracee, PR_mremap);
	poke_reg(tracee, SYSARG_1 /* old_address */, tracee->heap->base - HEAP_OFFSET);
	poke_reg(tracee, SYSARG_2 /* old_size    */, tracee->heap->size + HEAP_OFFSET);
	poke_reg(tracee, SYSARG_3 /* new_size    */, address - tracee->heap->base + HEAP_OFFSET);
	poke_reg(tracee, SYSARG_4 /* flags       */, 0);
	poke_reg(tracee, SYSARG_5 /* new_address */, 0);

	return 0;
}

void translate_brk_exit(Tracee *tracee)
{
	word_t result;
	word_t sysnum;
	int tracee_errno;

	sysnum = get_sysnum(tracee, MODIFIED);
	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	tracee_errno = (int) result;

	switch (sysnum) {
	case PR_void:
		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_mmap:
	case PR_mmap2:
		/* On error, mmap(2) returns -errno (the last 4k is
		 * reserved for this), whereas brk(2) returns the
		 * previous value.  */
		if (tracee_errno < 0 && tracee_errno > -4096) {
			poke_reg(tracee, SYSARG_RESULT, 0);
			break;
		}

		tracee->heap->base = result + HEAP_OFFSET;
		tracee->heap->size = 0;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_mremap:
		/* On error, mremap(2) returns -errno (the last 4k is
		 * reserved this), whereas brk(2) returns the previous
		 * value.  */
		if (   (tracee_errno < 0 && tracee_errno > -4096)
		    || (tracee->heap->base != result + HEAP_OFFSET)) {
			poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
			break;
		}

		tracee->heap->size = peek_reg(tracee, MODIFIED, SYSARG_3) - HEAP_OFFSET;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	default:
		assert(0);
	}

	DEBUG_BRK("brk() = 0x%lx\n", peek_reg(tracee, CURRENT, SYSARG_RESULT));
}

