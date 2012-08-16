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

#ifndef ARRAY_H
#define ARRAY_H

#include <stdbool.h>

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "arch.h"

struct array;
typedef int (*read_item_t)(struct array *array, size_t index, void **value);
typedef int (*write_item_t)(struct array *array, size_t index, const void *value);
typedef int (*compare_item_t)(struct array *array, size_t index, const void *reference);
typedef int (*sizeof_item_t)(struct array *array, size_t index);

struct _entry;
struct array {
	struct _entry *_cache;
	size_t length;

	struct tracee *tracee;

	read_item_t    read_item;
	write_item_t   write_item;
	compare_item_t compare_item;
	sizeof_item_t  sizeof_item;
};

/**
 * XXX
 */

static inline int read_item(struct array *array, size_t index, void **value)
{
	return array->read_item(array, index, value);
}


static inline int write_item(struct array *array, size_t index, const void *value)
{
	return array->write_item(array, index, value);
}

static inline int compare_item(struct array *array, size_t index, const void *reference)
{
	return array->compare_item(array, index, reference);
}

static inline int sizeof_item(struct array *array, size_t index)
{
	return array->sizeof_item(array, index);
}

extern int find_item(struct array *array, const void *reference);
extern int resize_array(struct array *array, size_t index, ssize_t nb_delta_entries);
extern int fetch_array(struct tracee *tracee, struct array *array, enum reg reg, size_t nb_entries);
extern int push_array(struct array *array, enum reg reg);
extern void free_array(struct array *array);

extern int read_item_data(struct array *array, size_t index, void **value);
extern int read_item_string(struct array *array, size_t index, char **value);
extern int write_item_string(struct array *array, size_t index, const char *value);
extern int write_items(struct array *array, size_t index, size_t nb_items, ...);
extern int compare_item_generic(struct array *array, size_t index, const void *reference);
extern int sizeof_item_string(struct array *array, size_t index);

#endif /* ARRAY_H */
