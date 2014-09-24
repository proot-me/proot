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

#ifndef SHEBANG_H
#define SHEBANG_H

#include <linux/limits.h>  /* PATH_MAX, ARG_MAX, */

#include "tracee/tracee.h"

extern int extract_shebang(const Tracee *tracee, const char *host_path, char user_path[PATH_MAX], char argument[ARG_MAX]);
extern int expand_shebang(Tracee *tracee, char host_path[PATH_MAX], char user_path[PATH_MAX]);

#endif /* SHEBANG_H */
