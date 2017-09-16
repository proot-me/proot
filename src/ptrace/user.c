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

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/user.h>
#include <stddef.h>

#include "ptrace/user.h"
#include "cli/note.h"

#if defined(ARCH_X86_64)

/**
 * Return the index in the "regs" field of a 64-bit "user" area that
 * corresponds to the specified @index in the "regs" field of a 32-bit
 * "user" area.
 */
static inline size_t convert_user_regs_index(size_t index)
{
	static size_t mapping[USER32_NB_REGS] = {
		05, /* ?bx */	11, /* ?cx */	12, /* ?dx */
		13, /* ?si */	14, /* ?di */	04, /* ?bp */
		10, /* ?ax */	23, /* ds */	24, /* es */
		25, /* fs */	26, /* gs */	15, /* orig_?ax */
		16, /* ?ip */	17, /* cs */	18, /* eflags */
		19, /* ?sp */	20, /* ss */ };

	/* Sanity check.  */
	assert(index < USER32_NB_REGS);

	return mapping[index];
}

/* Layout of a 32-bit "user" area.  */
#define USER32_REGS_OFFSET	0
#define USER32_REGS_SIZE	(USER32_NB_REGS * sizeof(uint32_t))
#define USER32_FPVALID_OFFSET	(USER32_REGS_OFFSET	+ USER32_REGS_SIZE)
#define USER32_I387_OFFSET	(USER32_FPVALID_OFFSET	+ sizeof(uint32_t))
#define USER32_I387_SIZE	(USER32_NB_FPREGS * sizeof(uint32_t))
#define USER32_TSIZE_OFFSET	(USER32_I387_OFFSET	+ USER32_I387_SIZE)
#define USER32_DSIZE_OFFSET	(USER32_TSIZE_OFFSET	+ sizeof(uint32_t))
#define USER32_SSIZE_OFFSET	(USER32_DSIZE_OFFSET	+ sizeof(uint32_t))
#define USER32_START_CODE_OFFSET  (USER32_SSIZE_OFFSET	+ sizeof(uint32_t))
#define USER32_START_STACK_OFFSET (USER32_START_CODE_OFFSET	+ sizeof(uint32_t))
#define USER32_SIGNAL_OFFSET	(USER32_START_STACK_OFFSET	+ sizeof(uint32_t))
#define USER32_RESERVED_OFFSET	(USER32_SIGNAL_OFFSET	+ sizeof(uint32_t))
#define USER32_AR0_OFFSET	(USER32_RESERVED_OFFSET	+ sizeof(uint32_t))
#define USER32_FPSTATE_OFFSET	(USER32_AR0_OFFSET	+ sizeof(uint32_t))
#define USER32_MAGIC_OFFSET	(USER32_FPSTATE_OFFSET	+ sizeof(uint32_t))
#define USER32_COMM_OFFSET	(USER32_MAGIC_OFFSET	+ sizeof(uint32_t))
#define USER32_COMM_SIZE	(32 * sizeof(uint8_t))
#define USER32_DEBUGREG_OFFSET	(USER32_COMM_OFFSET	+ USER32_COMM_SIZE)
#define USER32_DEBUGREG_SIZE	(8 * sizeof(uint32_t))

/**
 * Return the offset in the "debugreg" field of a 64-bit "user" area
 * that corresponds to the specified @offset in the "debugreg" field
 * of a 32-bit "user" area.
 */
static inline size_t convert_user_debugreg_offset(size_t offset)
{
	size_t index;

	/* Sanity check.  */
	assert(offset >= USER32_DEBUGREG_OFFSET
	    && offset < USER32_DEBUGREG_OFFSET + USER32_DEBUGREG_SIZE);

	index = (offset - USER32_DEBUGREG_OFFSET) / sizeof(uint32_t);
	return offsetof(struct user, u_debugreg) + index * sizeof(uint64_t);
}

/**
 * Return the offset in a 64-bit "user" area that corresponds to the
 * specified @offset in a 32-bit "user" area.  This function returns
 * "(word_t) -1" if the specified @offset is invalid.
 */
word_t convert_user_offset(word_t offset)
{
	const char *area_name = NULL;

	if (/* offset >= 0 && */ offset < USER32_REGS_OFFSET + USER32_REGS_SIZE) {
		/* Sanity checks.  */
		if ((offset % sizeof(uint32_t)) != 0)
			return (word_t) -1;

		return convert_user_regs_index(offset / sizeof(uint32_t)) * sizeof(uint64_t);
	}
	else if (offset == USER32_FPVALID_OFFSET)
		area_name = "fpvalid"; /* Not yet supported.  */
	else if (offset >= USER32_I387_OFFSET && offset < USER32_I387_OFFSET + USER32_I387_SIZE)
		area_name = "i387"; /* Not yet supported.  */
	else if (offset == USER32_TSIZE_OFFSET)
		area_name = "tsize"; /* Not yet supported.  */
	else if (offset == USER32_DSIZE_OFFSET)
		area_name = "dsize"; /* Not yet supported.  */
	else if (offset == USER32_SSIZE_OFFSET)
		area_name = "ssize"; /* Not yet supported.  */
	else if (offset == USER32_START_CODE_OFFSET)
		area_name = "start_code"; /* Not yet supported.  */
	else if (offset == USER32_START_STACK_OFFSET)
		area_name = "start_stack"; /* Not yet supported.  */
	else if (offset == USER32_SIGNAL_OFFSET)
		area_name = "signal"; /* Not yet supported.  */
	else if (offset == USER32_RESERVED_OFFSET)
		area_name = "reserved"; /* Not yet supported.  */
	else if (offset == USER32_AR0_OFFSET)
		area_name = "ar0"; /* Not yet supported.  */
	else if (offset == USER32_FPSTATE_OFFSET)
		area_name = "fpstate"; /* Not yet supported.  */
	else if (offset == USER32_MAGIC_OFFSET)
		area_name = "magic"; /* Not yet supported.  */
	else if (offset >= USER32_COMM_OFFSET && offset < USER32_COMM_OFFSET + USER32_COMM_SIZE)
		area_name = "comm"; /* Not yet supported.  */
	else if (offset >= USER32_DEBUGREG_OFFSET && offset < USER32_DEBUGREG_OFFSET + USER32_DEBUGREG_SIZE)
		return convert_user_debugreg_offset(offset);
	else
		area_name = "<unknown>";

	note(NULL, WARNING, INTERNAL, "ptrace user area '%s' not supported yet", area_name);
	return (word_t) -1;  /* Unknown offset.  */
}

/**
 * Convert the "regs" field from a 64-bit "user" area into a "regs"
 * field from a 32-bit "user" area, or vice versa according to
 * @reverse.
 */
void convert_user_regs_struct(bool reverse, uint64_t *user_regs64,
			uint32_t user_regs32[USER32_NB_REGS])
{
	size_t index32;

	for (index32 = 0; index32 < USER32_NB_REGS; index32++) {
		size_t index64 = convert_user_regs_index(index32);
		assert(index64 != (size_t) -1);

		if (reverse)
			user_regs64[index64] = (uint64_t) user_regs32[index32];
		else
			user_regs32[index32] = (uint32_t) user_regs64[index64];
	}
}

#endif /* ARCH_X86_64 */
