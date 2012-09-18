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

#ifndef LDSO_H
#define LDSO_H

#include <linux/limits.h>

#include "tracee/array.h"

extern int ldso_env_passthru(Array *envp, Array *argv, const char *define, const char *undefine);
extern int rebuild_host_ldso_paths(const Tracee *tracee, const char t_program[PATH_MAX], Array *envp);
extern int compare_item_env(Array *array, size_t index, const char *name);

#endif /* LDSO_H */
