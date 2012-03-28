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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h> /* bool, */

struct config {
	const char *guest_rootfs;
	const char *host_rootfs;
	const char *initial_cwd;
	const char *kernel_release;

	char **qemu;
	char **command;

	bool ignore_elf_interpreter;
	bool allow_unknown_syscalls;
	bool disable_aslr;
	bool fake_id0;

	bool check_fd;

	int verbose_level;
};

extern struct config config;

extern void print_config();

#endif /* CONFIG_H */
