/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2013 STMicroelectronics
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

#ifndef INTERP_H
#define INTERP_H

#include <linux/limits.h> /* PATH_MAX, ARG_MAX, */

#include "tracee/tracee.h"

typedef int (* extract_interp_t)(const Tracee *tracee, const char *t_path,
				 char u_interp[PATH_MAX], char argument[ARG_MAX]);

extern int extract_script_interp(const Tracee *tracee, const char *t_path,
				 char u_interp[PATH_MAX], char argument[ARG_MAX]);

extern int extract_elf_interp(const Tracee *tracee, const char *t_path,
				 char u_interp[PATH_MAX], char argument[ARG_MAX]);

#endif /* INTERP_H */
