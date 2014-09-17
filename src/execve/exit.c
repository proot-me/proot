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

#include <unistd.h>     /* sysconf(3), _SC*, */
#include <talloc.h>     /* talloc*, */

#include <sys/mman.h>   /* PROT_*, */

#include "execve/load.h"
#include "execve/elf.h"
#include "tracee/tracee.h"
#include "cli/notice.h"

static word_t page_size = 0;
static word_t page_mask = 0;

/**
 * Print to stderr the content of @load.
 */
static void print_load_map(const Tracee *tracee, const LoadMap *load)
{
	size_t length;
	size_t i;

	if (load == NULL || load->path == NULL || load->mappings == NULL)
		return;

	if (page_size == 0) {
		page_size = sysconf(_SC_PAGE_SIZE);
		if ((int) page_size <= 0)
			page_size = 0x1000;
		page_mask = ~(page_size - 1);
	}

	fprintf(stderr,
"	base = 0;							\n"
"	fd = open(\"%s\", O_RDONLY);					\n"
"	if (fd < 0) exit(1);						\n"
"									\n",
		load->path);

	length = talloc_array_length(load);
	for (i = 0; i <= length; i++) {
		word_t start_offset;
		word_t start_address;
		word_t end_address;
		word_t extra_address;
		word_t mmap_flags;

		/* Load addresses are aligned on a page boundary.
		 * About "extra_address": "If the segment's memory
		 * size p_memsz is larger than the file size p_filesz,
		 * the "extra" bytes are defined to hold the value 0
		 * and to follow the segment's initialized area." --
		 * man 7 elf.  */
		start_offset  = load->mappings[i].file.base & page_mask;
		start_address = load->mappings[i].mem.base & page_mask;
		end_address   = (load->mappings[i].mem.base + load->mappings[i].file.size + page_size) & page_mask;
		extra_address = (load->mappings[i].mem.base + load->mappings[i].mem.size + page_size) & page_mask;

		mmap_flags =  (0
			| (load->mappings[i].flags & PF_R ? PROT_READ  : 0)
			| (load->mappings[i].flags & PF_W ? PROT_WRITE : 0)
			| (load->mappings[i].flags & PF_X ? PROT_EXEC  : 0));

		fprintf(stderr,
"	addr = mmap((void *)(0x%lx + base), 0x%lx, 0x%lx, MAP_PRIVATE, fd, 0x%lx); \n",
			start_address, end_address - start_address, mmap_flags, start_offset);

		if (load->position_independent) {
			fprintf(stderr,
"	if ((void *)0x%lx == NULL) base = (unsigned long) addr;		\n",
				start_offset);
		}


		if (extra_address > end_address) {
			fprintf(stderr,
"	mmap((void *)0x%lx + base, 0x%lx, 0x%lx, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);\n",
				end_address, extra_address - end_address, mmap_flags);
		}
	}

	fprintf(stderr,
"	close(fd);							\n"
"									\n");

	if (load->interp != NULL)
		print_load_map(tracee, load->interp);
}

/**
 * TODO.  This function returns -errno if an error occured, otherwise
 * 0.
 */
int translate_execve_exit(Tracee *tracee)
{
	fprintf(stderr,
"#include <sys/types.h>							\n"
"#include <sys/stat.h>							\n"
"#include <fcntl.h>							\n"
"#include <stdlib.h>							\n"
"#include <sys/mman.h>							\n"
"#include <unistd.h>							\n"
"									\n"
"int main(void)								\n"
"{									\n"
"	int fd;								\n"
"	void *addr;							\n"
"	unsigned long base;						\n"
"									\n");

	print_load_map(tracee, tracee->load);

	fprintf(stderr,
"	while(1);							\n"
"	exit(1);							\n"
"}									\n");

	return 0;
}
