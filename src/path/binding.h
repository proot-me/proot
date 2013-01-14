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

#ifndef BINDING_H
#define BINDING_H

#include <limits.h> /* PATH_MAX, */
#include <stdbool.h>

#include "tracee/tracee.h"
#include "path.h"

typedef struct binding {
	Path host;
	Path guest;

	bool need_substitution;
	bool must_exist;

	struct {
		CIRCLEQ_ENTRY(binding) pending;
		CIRCLEQ_ENTRY(binding) guest;
		CIRCLEQ_ENTRY(binding) host;
	} link;
} Binding;

typedef CIRCLEQ_HEAD(bindings, binding) Bindings;

extern void insort_binding2(Tracee *tracee, Binding *binding);
extern Binding *new_binding(Tracee *tracee, const char *host, const char *guest, bool must_exist);
extern int initialize_bindings(Tracee *tracee);
extern const char *get_path_binding(Tracee* tracee, Side side, const char path[PATH_MAX]);
extern const char *get_root(const Tracee* tracee);
extern int substitute_binding(Tracee* tracee, Side side, char path[PATH_MAX]);

#endif /* BINDING_H */
