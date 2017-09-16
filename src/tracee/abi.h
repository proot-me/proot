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

#ifndef TRACEE_ABI_H
#define TRACEE_ABI_H

#include <stdbool.h>
#include <stddef.h> /* offsetof(),  */

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "arch.h"

#include "attribute.h"

typedef enum {
	ABI_DEFAULT = 0,
	ABI_2, /* x86_32 on x86_64.  */
	ABI_3, /* x32 on x86_64.  */
	NB_MAX_ABIS,
} Abi;

/**
 * Return the ABI currently used by the given @tracee.
 */
#if defined(ARCH_X86_64)
static inline Abi get_abi(const Tracee *tracee)
{
	/* The ABI can be changed by a syscall ("execve" typically),
	 * however the change is only effective once the syscall has
	 * *fully* returned, hence the use of _regs[ORIGINAL].  */
	switch (tracee->_regs[ORIGINAL].cs) {
	case 0x23:
		return ABI_2;

	case 0x33:
		if (tracee->_regs[ORIGINAL].ds == 0x2B)
			return ABI_3;
		/* Fall through.  */
	default:
		return ABI_DEFAULT;
	}
}

/**
 * Return true if @tracee is a 32-bit process running on a 64-bit
 * kernel.
 */
static inline bool is_32on64_mode(const Tracee *tracee)
{
	/* Unlike the ABI, 32-bit/64-bit mode change is effective
	 * immediately, hence _regs[CURRENT].cs.  */
	switch (tracee->_regs[CURRENT].cs) {
	case 0x23:
		return true;

	case 0x33:
		if (tracee->_regs[CURRENT].ds == 0x2B)
			return true;
		/* Fall through.  */
	default:
		return false;
	}
}
#else
static inline Abi get_abi(const Tracee *tracee UNUSED)
{
	return ABI_DEFAULT;
}

static inline bool is_32on64_mode(const Tracee *tracee UNUSED)
{
	return false;
}
#endif

/**
 * Return the size of a word according to the ABI currently used by
 * the given @tracee.
 */
static inline size_t sizeof_word(const Tracee *tracee)
{
	return (is_32on64_mode(tracee)
		? sizeof(word_t) / 2
		: sizeof(word_t));
}

#include <sys/stat.h>

/**
 * Return the offset of the 'uid' field in a 'stat' structure
 * according to the ABI currently used by the given @tracee.
 */
static inline off_t offsetof_stat_uid(const Tracee *tracee)
{
	return (is_32on64_mode(tracee)
		? OFFSETOF_STAT_UID_32
		: offsetof(struct stat, st_uid));
}

/**
 * Return the offset of the 'gid' field in a 'stat' structure
 * according to the ABI currently used by the given @tracee.
 */
static inline off_t offsetof_stat_gid(const Tracee *tracee)
{
	return (is_32on64_mode(tracee)
		? OFFSETOF_STAT_GID_32
		: offsetof(struct stat, st_gid));
}

#endif /* TRACEE_ABI_H */
