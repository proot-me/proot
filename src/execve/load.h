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

#ifndef LOAD_H
#define LOAD_H

#include <stdbool.h>

#include "tracee/tracee.h"
#include "execve/elf.h"
#include "arch.h"

typedef struct mapping {
	word_t addr;
	word_t length;
	word_t clear_length;
	word_t prot;
	word_t flags;
	word_t fd;
	word_t offset;
} Mapping;

typedef struct load_info {
	char *path;
	Mapping *mappings;
	ElfHeader elf_header;

	struct load_info *interp;
} LoadInfo;

extern void translate_load_enter(Tracee *tracee);
extern void translate_load_exit(Tracee *tracee);

#endif /* LOAD_H */
