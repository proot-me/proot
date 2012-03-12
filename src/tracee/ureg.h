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

#include <sys/types.h> /* off_t */

#include "tracee/info.h"

#define UREGS_LENGTH 9
extern off_t uregs[UREGS_LENGTH];

#if defined(ARCH_X86_64)
extern off_t uregs2[UREGS_LENGTH];
#endif

extern word_t peek_ureg(struct tracee_info *tracee, int index);
extern int poke_ureg(struct tracee_info *tracee, int index, word_t value);

#endif /* UREG_H */
