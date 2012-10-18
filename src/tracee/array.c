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

#include <linux/limits.h> /* ARG_MAX, */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strlen(3), memcmp(3), memcpy(3), */
#include <strings.h>  /* bzero(3), */
#include <stdbool.h>  /* bool, true, false, */
#include <errno.h>    /* E*,  */
#include <stdarg.h>   /* va_*, */
#include <stdint.h>   /* uint32_t, */
#include <talloc.h>   /* talloc_*, */

#include "arch.h"

struct array_entry {
	/* Pointer (tracee's address space) to the current value, if
	 * local == NULL.  */
	word_t remote;

	/* Pointer (tracer's address space) to the current value, if
	 * local != NULL.  */
	void *local;
};

#include "tracee/tracee.h"
#include "tracee/array.h"
#include "tracee/mem.h"
#include "tracee/abi.h"
#include "build.h"

/**
 * Set *@value to the address of the data pointed to by the entry in
 * @array at the given @index.  This function returns -errno when an
 * error occured, otherwise 0.
 */
int read_item_data(Array *array, size_t index, void **value)
{
	int status;
	int size;

	assert(index < array->length);

	/* Already cached locally?  */
	if (array->_cache[index].local != NULL)
		goto end;

	/* Remote NULL is mapped to local NULL.  */
	if (array->_cache[index].remote == 0) {
		array->_cache[index].local = NULL;
		goto end;
	}

	size = sizeof_item(array, index);
	if (size < 0)
		return size;

	array->_cache[index].local = talloc_size(array, size);
	if (array->_cache[index].local == NULL)
		return -ENOMEM;

	/* Copy locally the remote data.  */
	status = read_data(TRACEE(array), array->_cache[index].local,
			   array->_cache[index].remote, size);
	if (status < 0) {
		array->_cache[index].local = NULL;
		return status;
	}

end:
	*value = array->_cache[index].local;
	return 0;
}

/**
 * Set *@value to the address of the string pointed to by the entry in
 * @array at the given @index.  This function returns -errno when an
 * error occured, otherwise 0.
 */
int read_item_string(Array *array, size_t index, char **value)
{
	char tmp[ARG_MAX];
	int status;

	assert(index < array->length);

	/* Already cached locally?  */
	if (array->_cache[index].local != NULL)
		goto end;

	/* Remote NULL is mapped to local NULL.  */
	if (array->_cache[index].remote == 0) {
		array->_cache[index].local = NULL;
		goto end;
	}

	/* Copy locally the remote string into a temporary buffer.  */
	status = read_string(TRACEE(array), tmp, array->_cache[index].remote, ARG_MAX);
	if (status < 0)
		return status;
	if (status >= ARG_MAX)
		return -ENOMEM;

	/* Save the local string in a "persistent" buffer.  */
	array->_cache[index].local = talloc_strdup(array, tmp);
	if (array->_cache[index].local == NULL)
		return -ENOMEM;

end:
	*value = array->_cache[index].local;
	return 0;
}

/**
 * This function returns the number of bytes of the string pointed to
 * by the entry in @array at the given @index, otherwise -errno when
 * an error occured.
 */
int sizeof_item_string(Array *array, size_t index)
{
	char *value;
	int status;

	assert(index < array->length);

	status = read_item_string(array, index, &value);
	if (status < 0)
		return status;

	if (value == NULL)
		return 0;

	return strlen(value) + 1;
}

/**
 * This function returns 1 or 0 depending on the equivalence of the
 * @reference item and the one pointed to by the entry in @array at
 * the given @index, otherwise it returns -errno when an error
 * occured.
 */
int compare_item_generic(Array *array, size_t index, const void *reference)
{
	void *value;
	int status;

	assert(index < array->length);

	status = read_item(array, index, &value);
	if (status < 0)
		return status;

	if (value == NULL && reference == NULL)
		return 1;

	if (value == NULL && reference != NULL)
		return 0;

	if (value != NULL && reference == NULL)
		return 0;

	status = sizeof_item(array, index);
	if (status < 0)
		return status;

	return (int)(memcmp(value, reference, status) == 0);
}

/**
 * This function returns the index in @array of the first item
 * equivalent to the @reference item, otherwise it returns -errno when
 * an error occured.
 */
int find_item(Array *array, const void *reference)
{
	int i;

	for (i = 0; i < array->length; i++) {
		int status;

		status = compare_item(array, i, reference);
		if (status < 0)
			return status;
		if (status != 0)
			break;
	}

	return i;
}

/**
 * Make the entry at the given @index in @array points to a copy of
 * the string pointed to by @value.  This function returns -errno when
 * an error occured, otherwise 0.
 */
int write_item_string(Array *array, size_t index, const char *value)
{
	assert(index < array->length);

	array->_cache[index].local = talloc_strdup(array, value);
	if (array->_cache[index].local == NULL)
		return -ENOMEM;

	return 0;
}

/**
 * Make the @nb_items entries at the given @index in @array points to
 * a copy of the item pointed to by the variadic arguments.  This
 * function returns -errno when an error occured, otherwise 0.
 */
int write_items(Array *array, size_t index, size_t nb_items, ...)
{
	va_list va_items;
	int status;
	int i;

	va_start(va_items, nb_items);

	for (i = 0; i < nb_items; i++) {
		void *value = va_arg(va_items, void *);

		status = write_item(array, index + i, value);
		if (status < 0)
			goto end;
	}
	status = 0;

end:
	va_end(va_items);
	return status;
}


/**
 * Resize the @array at the given @index by the @delta_nb_entries.
 * Please, note that *no* item is de/allocated, the caller has to call
 * free/write_item by iteself.  This function returns -errno when an
 * error occured, otherwise 0.
 */
int resize_array(Array *array, size_t index, ssize_t delta_nb_entries)
{
	size_t nb_moved_entries;
	size_t new_length;
	void *tmp;

	assert(index < array->length);

	if (delta_nb_entries == 0)
		return 0;

	new_length = array->length + delta_nb_entries;
	nb_moved_entries = array->length - index;

	if (delta_nb_entries > 0) {
		tmp = talloc_realloc(array, array->_cache, ArrayEntry, new_length);
		if (tmp == NULL)
			return -ENOMEM;
		array->_cache = tmp;

		memmove(array->_cache + index + delta_nb_entries, array->_cache + index,
			nb_moved_entries * sizeof(ArrayEntry));

		bzero(array->_cache + index, delta_nb_entries * sizeof(ArrayEntry));
	}
	else {
		assert(index + delta_nb_entries >= 0);

		memmove(array->_cache + index + delta_nb_entries, array->_cache + index,
			nb_moved_entries * sizeof(ArrayEntry));

		tmp = talloc_realloc(array, array->_cache, ArrayEntry, new_length);
		if (tmp == NULL)
			return -ENOMEM;
		array->_cache = tmp;
	}

	array->length = new_length;
	return 0;
}

/**
 * Copy and cache into @array the pointer table pointed to by @reg
 * from the @tracee memory space general.  Only the first @nb_entries
 * are copied, unless it is 0 then all the entries up to the NULL
 * pointer are copied.  This function returns -errno when an error
 * occured, otherwise 0.
 */
int fetch_array(Array *array, Reg reg, size_t nb_entries)
{
	word_t pointer = (word_t)-1;
	word_t address;
	int i;

	assert(array != NULL);
	assert(array->_cache == NULL);

	address = peek_reg(TRACEE(array), CURRENT, reg);

	for (i = 0; nb_entries != 0 ? i < nb_entries : pointer != 0; i++) {
		void *tmp = talloc_realloc(array, array->_cache, ArrayEntry, i + 1);
		if (tmp == NULL)
			return -ENOMEM;
		array->_cache = tmp;

		pointer = peek_mem(TRACEE(array), address + i * sizeof_word(TRACEE(array)));
		if (errno != 0)
			return -EFAULT;

		array->_cache[i].remote = pointer;
		array->_cache[i].local = NULL;
	}
	array->length = i;

	/* By default, assume it is an array of string pointers.  */
	array->read_item   = (read_item_t)read_item_string;
	array->sizeof_item = sizeof_item_string;
	array->write_item  = (write_item_t)write_item_string;

	/* By default, use generic callbacks: they rely on
	 * array->read_item() and array->sizeof_item().  */
	array->compare_item = compare_item_generic;

	return 0;
}

/**
 * Copy -- if needed -- the pointer table and new items cached by
 * @array into the tracee's memory, and make @reg points to the new
 * pointer table.  This function returns -errno if an error occured,
 * otherwise 0.
 */
int push_array(Array *array, Reg reg)
{
	Tracee *tracee;
	struct iovec *local;
	size_t local_count;
	size_t total_size;
	word_t *pod_array;
	word_t tracee_ptr;
	int status;
	int i;

	/* Nothing to do, for sure.  */
	if (array == NULL)
		return 0;

	tracee = TRACEE(array);

	/* The pointer table is a POD array in the tracee's memory.  */
	pod_array = talloc_zero_size(tracee->tmp, array->length * sizeof_word(TRACEE(array)));
	if (pod_array == NULL)
		return -ENOMEM;

	/* There's one vector per modified item + one vector for the
	 * pod array.  */
	local = talloc_zero_array(tracee->tmp, struct iovec, array->length + 1);
	if (local == NULL)
		return -ENOMEM;

	/* The pod array is expected to be at the beginning of the
	 * allocated memory by the caller.  */
	total_size = array->length * sizeof_word(TRACEE(array));
	local[0].iov_base = pod_array;
	local[0].iov_len  = total_size;
	local_count = 1;

	/* Create one vector for each modified item.  */
	for (i = 0; i < array->length; i++) {
		ssize_t size;

		if (array->_cache[i].local == NULL)
			continue;

		/* At this moment, we only know the offsets in the
		 * tracee's memory block. */
		array->_cache[i].remote = total_size;

		size = sizeof_item(array, i);
		if (size < 0)
			return size;
		total_size += size;

		local[local_count].iov_base = array->_cache[i].local;
		local[local_count].iov_len  = size;
		local_count++;
	}

	/* Nothing has changed, don't update anything.  */
	if (local_count == 1)
		return 0;
	assert(local_count < array->length + 1);

	/* Modified items and the pod array are stored in a tracee's
	 * memory block.  */
	tracee_ptr = alloc_mem(TRACEE(array), total_size);
	if (tracee_ptr == 0)
		return -E2BIG;

	/* Now, we know the absolute addresses in the tracee's
	 * memory.  */
	for (i = 0; i < array->length; i++) {
		if (array->_cache[i].local != NULL)
			array->_cache[i].remote += tracee_ptr;

		if (is_32on64_mode(TRACEE(array)))
			((uint32_t *)pod_array)[i] = array->_cache[i].remote;
		else
			pod_array[i] = array->_cache[i].remote;
	}

	/* Write all the modified items and the pod array at once.  */
	status = writev_data(TRACEE(array), tracee_ptr, local, local_count);
	if (status < 0)
		return status;

	poke_reg(TRACEE(array), reg, tracee_ptr);
	return 0;
}
