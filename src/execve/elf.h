/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 * Inspired by: elf(5) from the man-page project.
 */

#ifndef ELF_H
#define ELF_H

#define EI_NIDENT 16

#include <stdint.h>

struct elf_header32 {
	unsigned char e_ident[EI_NIDENT];
	uint16_t      e_type;
	uint16_t      e_machine;
	uint32_t      e_version;
	uint32_t      e_entry;
	uint32_t      e_phoff;
	uint32_t      e_shoff;
	uint32_t      e_flags;
	uint16_t      e_ehsize;
	uint16_t      e_phentsize;
	uint16_t      e_phnum;
	uint16_t      e_shentsize;
	uint16_t      e_shnum;
	uint16_t      e_shstrndx;
};

struct elf_header64 {
	unsigned char e_ident[EI_NIDENT];
	uint16_t      e_type;
	uint16_t      e_machine;
	uint32_t      e_version;
	uint64_t      e_entry;
	uint64_t      e_phoff;
	uint64_t      e_shoff;
	uint32_t      e_flags;
	uint16_t      e_ehsize;
	uint16_t      e_phentsize;
	uint16_t      e_phnum;
	uint16_t      e_shentsize;
	uint16_t      e_shnum;
	uint16_t      e_shstrndx;
};

union elf_header {
	struct elf_header32 class32;
	struct elf_header64 class64;
};

struct program_header32 {
	uint32_t   p_type;
	uint32_t   p_offset;
	uint32_t   p_vaddr;
	uint32_t   p_paddr;
	uint32_t   p_filesz;
	uint32_t   p_memsz;
	uint32_t   p_flags;
	uint32_t   p_align;
};

struct program_header64 {
	uint32_t   p_type;
	uint32_t   p_flags;
	uint64_t   p_offset;
	uint64_t   p_vaddr;
	uint64_t   p_paddr;
	uint64_t   p_filesz;
	uint64_t   p_memsz;
	uint64_t   p_align;
};

union program_header {
	struct program_header32 class32;
	struct program_header64 class64;
};

/* The following macros are also compatible with ELF 64-bit. */
#define ELF_IDENT(header, index) (header).class32.e_ident[(index)]
#define ELF_CLASS(header) ELF_IDENT(header, 4)
#define IS_CLASS32(header) (ELF_CLASS(header) == 1)
#define IS_CLASS64(header) (ELF_CLASS(header) == 2)

/* Helper to access a @field of the structure elf_headerXX. */
#define ELF_FIELD(header, field)		\
	(IS_CLASS64(header)			\
	 ? (header).class64. e_ ## field	\
	 : (header).class32. e_ ## field)

/* Helper to access a @field of the structure program_headerXX */
#define PROGRAM_FIELD(ehdr, phdr, field)	\
	(IS_CLASS64(ehdr)			\
	 ? (phdr).class64. p_ ## field		\
	 : (phdr).class32. p_ ## field)

#define IS_INTERP(ehdr, phdr) (PROGRAM_FIELD(ehdr, phdr, type) == 3)

#define KNOWN_PHENTSIZE(header, size)					\
	(   (IS_CLASS32(header) && (size) == sizeof(struct program_header32)) \
	 || (IS_CLASS64(header) && (size) == sizeof(struct program_header64)))

#endif /* ELF_H */
