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

#include "tracee/reg.h"
#include "arch.h"

typedef struct array Array;
typedef int (*read_item_t)(Array *array, size_t index, void **value);
typedef int (*write_item_t)(Array *array, size_t index, const void *value);
typedef int (*compare_item_t)(Array *array, size_t index, const void *reference);
typedef int (*sizeof_item_t)(Array *array, size_t index);

typedef struct array_entry ArrayEntry;
struct array {
	ArrayEntry *_cache;
	size_t length;

	read_item_t    read_item;
	write_item_t   write_item;
	compare_item_t compare_item;
	sizeof_item_t  sizeof_item;
};

static inline int read_item(Array *array, size_t index, void **value)
{
	return array->read_item(array, index, value);
}

static inline int write_item(Array *array, size_t index, const void *value)
{
	return array->write_item(array, index, value);
}

static inline int compare_item(Array *array, size_t index, const void *reference)
{
	return array->compare_item(array, index, reference);
}

static inline int sizeof_item(Array *array, size_t index)
{
	return array->sizeof_item(array, index);
}

extern int find_item(Array *array, const void *reference);
extern int resize_array(Array *array, size_t index, ssize_t nb_delta_entries);
extern int fetch_array(Array *array, Reg reg, size_t nb_entries);
extern int push_array(Array *array, Reg reg);

extern int read_item_data(Array *array, size_t index, void **value);
extern int read_item_string(Array *array, size_t index, char **value);
extern int write_item_string(Array *array, size_t index, const char *value);
extern int write_items(Array *array, size_t index, size_t nb_items, ...);
extern int compare_item_generic(Array *array, size_t index, const void *reference);
extern int sizeof_item_string(Array *array, size_t index);

#endif /* ARRAY_H */
