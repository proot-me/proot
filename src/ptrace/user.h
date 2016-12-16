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

#include "arch.h"
#include "attribute.h"

#if defined(ARCH_X86_64)

#define USER32_NB_REGS   17
#define USER32_NB_FPREGS 27

extern word_t convert_user_offset(word_t offset);
extern void convert_user_regs_struct(bool reverse, uint64_t *user_regs64,
				uint32_t user_regs32[USER32_NB_REGS]);

#else

#define USER32_NB_REGS   0
#define USER32_NB_FPREGS 0

static inline word_t convert_user_offset(word_t offset UNUSED)
{
	assert(0);
}

static inline void convert_user_regs_struct(bool reverse UNUSED,
					uint64_t *user_regs64 UNUSED,
					uint32_t user_regs32[USER32_NB_REGS] UNUSED)
{
	assert(0);
}

#endif
