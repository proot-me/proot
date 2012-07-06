/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
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

#ifndef UREG_H
#define UREG_H

#include "tracee/info.h"
#include "arch.h"

enum ureg {
	SYSARG_NUM = 0,
	SYSARG_1,
	SYSARG_2,
	SYSARG_3,
	SYSARG_4,
	SYSARG_5,
	SYSARG_6,
	SYSARG_RESULT,
	STACK_POINTER,

	/* Helpers. */
	UREG_FIRST = SYSARG_NUM,
	UREG_LAST  = STACK_POINTER,
};

extern int fetch_uregs(struct tracee_info *tracee);
extern int push_uregs(struct tracee_info *tracee);
extern word_t peek_ureg(const struct tracee_info *tracee, enum ureg ureg);
extern void poke_ureg(struct tracee_info *tracee, enum ureg ureg, word_t value);

enum abi {
	ABI_DEFAULT,
#if defined(ARCH_X86_64)
	ABI_X86,
#endif
};

enum abi get_abi(const struct tracee_info *tracee);

#endif /* UREG_H */
