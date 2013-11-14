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

#include <sys/mman.h>	/* PROT_*, MAP_*, */
#include <assert.h>	/* assert(3),  */
#include <string.h>     /* strerror(3), */
#include <unistd.h>     /* sysconf(3), */
#include <sys/param.h>  /* MIN(), MAX(), */

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "syscall/sysnum.h"
#include "cli/notice.h"

#include "compat.h"

#define DEBUG_BRK(...) /* fprintf(stderr, __VA_ARGS__) */

/* The size of the heap can be zero, unlike the size of a memory
 * mapping.  As a consequence, the first page of the "heap" memory
 * mapping is discarded in order to emulate an empty heap.  */
static word_t heap_offset = 0;

#define PREALLOCATED_HEAP_SIZE (8 * 1024 * 1024)

word_t translate_brk_enter(Tracee *tracee)
{
	word_t new_brk_address;
	size_t old_heap_size;
	size_t new_heap_size;

	if (heap_offset == 0) {
		heap_offset = sysconf(_SC_PAGE_SIZE);
		if ((int) heap_offset <= 0)
			heap_offset = 0x1000;
	}

	/* Non-fixed mmap pages might be placed right after the
	 * emulated heap on some architectures.  A solution is to
	 * preallocate some space to ensure a minimal heap size.  */
	if (tracee->heap->prealloc_size == 0)
		tracee->heap->prealloc_size = MAX(PREALLOCATED_HEAP_SIZE, heap_offset);

	new_brk_address = peek_reg(tracee, CURRENT, SYSARG_1);
	DEBUG_BRK("brk(0x%lx)\n", new_brk_address);

	/* Allocate a new mapping for the emulated heap.  */
	if (tracee->heap->base == 0) {
		Sysnum sysnum;

		if (new_brk_address != 0)
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
		poke_reg(tracee, SYSARG_2 /* length  */, heap_offset + tracee->heap->prealloc_size);
		poke_reg(tracee, SYSARG_3 /* prot    */, PROT_READ | PROT_WRITE);
		poke_reg(tracee, SYSARG_4 /* flags   */, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT);
		poke_reg(tracee, SYSARG_5 /* fd      */, -1);
		poke_reg(tracee, SYSARG_6 /* offset  */, 0);

		return 0;
	}

	/* The size of the heap can't be negative.  */
	if (new_brk_address < tracee->heap->base) {
		set_sysnum(tracee, PR_void);
		return 0;
	}

	new_heap_size = new_brk_address - tracee->heap->base;
	old_heap_size = tracee->heap->size;

	/* Clear the released memory in preallocated space, so it will be
	 * in the expected state next time it will be reallocated.  */
	if (new_heap_size < old_heap_size && new_heap_size < tracee->heap->prealloc_size) {
		(void) clear_mem(tracee,
				tracee->heap->base + new_heap_size,
				MIN(old_heap_size, tracee->heap->prealloc_size) - new_heap_size);
	}

	/* No need to use mremap when both old size and new size are
	 * in the preallocated space.  */
	if (   new_heap_size <= tracee->heap->prealloc_size
	    && old_heap_size <= tracee->heap->prealloc_size) {
		tracee->heap->size = new_heap_size;
		set_sysnum(tracee, PR_void);
		return 0;
	}

	/* Ensure the preallocated space will never be released.  */
	new_heap_size = MAX(new_heap_size, tracee->heap->prealloc_size);
	old_heap_size = MAX(old_heap_size, tracee->heap->prealloc_size);

	/* Actually resizing.  */
	set_sysnum(tracee, PR_mremap);
	poke_reg(tracee, SYSARG_1 /* old_address */, tracee->heap->base - heap_offset);
	poke_reg(tracee, SYSARG_2 /* old_size    */, old_heap_size + heap_offset);
	poke_reg(tracee, SYSARG_3 /* new_size    */, new_heap_size + heap_offset);
	poke_reg(tracee, SYSARG_4 /* flags       */, 0);
	poke_reg(tracee, SYSARG_5 /* new_address */, 0);

	return 0;
}

void translate_brk_exit(Tracee *tracee)
{
	word_t result;
	word_t sysnum;
	int tracee_errno;

	assert(heap_offset > 0);

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

		tracee->heap->base = result + heap_offset;
		tracee->heap->size = 0;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_mremap:
		/* On error, mremap(2) returns -errno (the last 4k is
		 * reserved this), whereas brk(2) returns the previous
		 * value.  */
		if (   (tracee_errno < 0 && tracee_errno > -4096)
		    || (tracee->heap->base != result + heap_offset)) {
			poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
			break;
		}

		tracee->heap->size = peek_reg(tracee, MODIFIED, SYSARG_3) - heap_offset;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	default:
		assert(0);
	}

	DEBUG_BRK("brk() = 0x%lx\n", peek_reg(tracee, CURRENT, SYSARG_RESULT));
}
