/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2014 STMicroelectronics
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

#ifndef HEAP_H
#define HEAP_H

#include "tracee/tracee.h"

extern int translate_brk_enter(Tracee *tracee);
extern void translate_brk_exit(Tracee *tracee);

/**
 * Check if the @address is in the @tracee's preallocated heap space.
 * This space is not supposed to be accessible.
 */
static inline bool belongs_to_heap_prealloc(const Tracee *tracee, word_t address)
{
	return (tracee->heap != NULL && !tracee->heap->disabled
		&& address >= tracee->heap->base + tracee->heap->size
		&& address < tracee->heap->base + tracee->heap->prealloc_size);
}

#endif /* HEAP_H */
