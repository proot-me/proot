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

#ifndef BINDING_H
#define BINDING_H

#include <limits.h> /* PATH_MAX, */
#include <stdbool.h>

#include "path.h"

extern int bind_path(const char *host_path, const char *guest_path, bool must_exist);
extern mode_t build_glue_rootfs(const char *guest_path, char host_path[PATH_MAX],
				enum finality is_final);
extern void print_bindings(void);
extern void free_bindings(void);

enum binding_side {
	GUEST_SIDE = 1,
	HOST_SIDE = 2,
};

extern const char *get_path_binding(enum binding_side side, const char path[PATH_MAX]);
extern int substitute_binding(enum binding_side side, char path[PATH_MAX]);


#endif /* BINDING_H */
