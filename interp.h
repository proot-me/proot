/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
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
 */

#ifndef INTERP_H
#define INTERP_H

#include "child_info.h"

typedef int (* expand_interp_t)(struct child_info *child, const char *path, char filename[PATH_MAX], char **argv[]);

extern int expand_script_interp(struct child_info *child, const char *path, char filename[PATH_MAX], char **argv[]);
extern int expand_elf_interp(struct child_info *child, const char *path, char filename[PATH_MAX], char **argv[]);

#endif /* INTERP_H */
