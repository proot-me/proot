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
#include <stdlib.h>   /* malloc(3), free(3), realloc(3), */
#include <strings.h>  /* bzero(3), */
#include <stdbool.h>  /* bool, true, false, */
#include <errno.h>    /* E*,  */
#include <stdarg.h>

#define ARRAY_INTERNALS
#include "tracee/array.h"
#undef ARRAY_INTERNALS

#include "tracee/mem.h"
#include "tracee/abi.h"

/**
 * Free the item pointed to by the entry in @array at the given
 * @index, then nullify this entry.
 */
static inline void free_item(struct array *array, size_t index)
{
	assert(index < array->length);

	if (array->_cache[index].local != NULL)
		free(array->_cache[index].local);
	array->_cache[index].remote = 0;
	array->_cache[index].local = NULL;
}

/**
 * Set *@value to the address of the data pointed to by the entry in
 * @array at the given @index.  This function returns -errno when an
 * error occured, otherwise 0.
 */
int read_item_data(struct array *array, size_t index, void **value)
{
	int status;
	int size;

	assert(index < array->length);

	/* Already cached locally?  */
	if (array->_cache[index].local != NULL)
		goto end;

	/* Remote NULL is mapped to local NULL.  */
	if (array->_cache[index].remote == 0) {
		free_item(array, index);
		goto end;
	}

	size = sizeof_item(array, index);
	if (size < 0)
		return size;

	array->_cache[index].local = malloc(size);
	if (array->_cache[index].local == NULL)
		return -ENOMEM;

	/* Copy locally the remote data.  */
	status = read_data(array->tracee, array->_cache[index].local,
			   array->_cache[index].remote, size);
	if (status < 0) {
		free(array->_cache[index].local);
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
int read_item_string(struct array *array, size_t index, char **value)
{
	char tmp[ARG_MAX];
	int status;

	assert(index < array->length);

	/* Already cached locally?  */
	if (array->_cache[index].local != NULL)
		goto end;

	/* Remote NULL is mapped to local NULL.  */
	if (array->_cache[index].remote == 0) {
		free_item(array, index);
		goto end;
	}

	/* Copy locally the remote string into a temporary buffer.  */
	status = read_string(array->tracee, tmp, array->_cache[index].remote, ARG_MAX);
	if (status < 0)
		return status;
	if (status >= ARG_MAX)
		return -ENOMEM;

	/* Save the local string in a "persistent" buffer.  */
	array->_cache[index].local = strdup(tmp);
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
int sizeof_item_string(struct array *array, size_t index)
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
int compare_item_generic(struct array *array, size_t index, const void *reference)
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
int find_item(struct array *array, const void *reference)
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
int write_item_string(struct array *array, size_t index, const char *value)
{
	int size;

	assert(index < array->length);

	free_item(array, index);

	size = strlen(value) + 1;
	array->_cache[index].local = malloc(size);
	if (array->_cache[index].local == NULL)
		return -ENOMEM;

	memcpy(array->_cache[index].local, value, size);

	return 0;
}

/**
 * Make the @nb_items entries at the given @index in @array points to
 * a copy of the item pointed to by the variadic arguments.  This
 * function returns -errno when an error occured, otherwise 0.
 */
int write_items(struct array *array, size_t index, size_t nb_items, ...)
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
int resize_array(struct array *array, size_t index, ssize_t delta_nb_entries)
{
	size_t nb_moved_entries;
	size_t new_length;
	void *tmp;
	int i;

	assert(index < array->length);

	if (delta_nb_entries == 0)
		return 0;

	new_length = array->length + delta_nb_entries;
	nb_moved_entries = array->length - index;

	if (delta_nb_entries > 0) {
		tmp = realloc(array->_cache, new_length * sizeof(struct _entry));
		if (tmp == NULL)
			return -ENOMEM;
		array->_cache = tmp;

		memmove(array->_cache + index + delta_nb_entries, array->_cache + index,
			nb_moved_entries * sizeof(struct _entry));

		bzero(array->_cache + index, delta_nb_entries * sizeof(struct _entry));
	}
	else {
		/* Sanity check.  */
		for (i = 0; i < nb_moved_entries; i++)
			assert(array->_cache[index + i].local == NULL);

		memmove(array->_cache + index + delta_nb_entries, array->_cache + index,
			nb_moved_entries * sizeof(struct _entry));

		tmp = realloc(array->_cache, new_length * sizeof(struct _entry));
		if (tmp == NULL)
			return -ENOMEM;
		array->_cache = tmp;
	}

	array->length = new_length;
	return 0;
}

/**
 * Copy and cache into @array the pointer table pointed to by @address
 * from the @tracee memory space general.  Only the first @nb_entries
 * are copied, unless it is 0 then all the entries up to the NULL
 * pointer are copied.  This function returns -errno when an error
 * occured, otherwise 0.
 */
int fetch_array(struct tracee *tracee, struct array *array, word_t address, size_t nb_entries)
{
	word_t pointer = (word_t)-1;
	int status;
	int i;

	assert(array != NULL);
	assert(array->_cache == NULL);

	for (i = 0; nb_entries != 0 ? i < nb_entries : pointer != 0; i++) {
		void *tmp = realloc(array->_cache, (i + 1) * sizeof(struct _entry));
		if (tmp == NULL) {
			status = -ENOMEM;
			goto end;
		}
		array->_cache = tmp;

		pointer = peek_mem(tracee, address + i * sizeof_word(tracee));
		if (errno != 0) {
			status = -EFAULT;
			goto end;
		}

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

	array->tracee = tracee;
	status = 0;

end:
	return status;
}

/**
 * Copy -- if needed -- the pointer table and new items cached by
 * @array into the tracee's stack.  This function returns the number
 * of bytes used in tracee's stack, otherwise it returns -errno when
 * an error occured.
 */
int push_array(struct array *array)
{
	size_t total_size = 0;

	struct tracee *tracee;
	word_t stack_pointer;
	int i;

	if (array->length == 0)
		return 0;

	tracee = array->tracee;

	/* Store the pointed items (strings, structures, whatever)
	 * inside the tracee's stack.  */
	for (i = 0; i < array->length; i++) {
		ssize_t size;

		if (array->_cache[i].local == NULL)
			continue;

		size = sizeof_item(array, i);
		if (size < 0)
			return size;
		total_size += size;

		stack_pointer = resize_stack(tracee, size);
		if (stack_pointer == 0)
			return -E2BIG;

		write_data(tracee, stack_pointer, array->_cache[i].local, size);

		/* Invalidate the locally cached value.  */
		free_item(array, i);
		array->_cache[i].remote = stack_pointer;
	}

	/* Nothing was changed, don't update anything.  */
	if (total_size == 0)
		return 0;

	/* Store the pointer table inside the tracee's stack.  */
	stack_pointer = resize_stack(tracee, array->length * sizeof_word(tracee));
	for (i = 0; i < array->length; i++)
		poke_mem(tracee, stack_pointer + i * sizeof_word(tracee), array->_cache[i].remote);

	return total_size;
}

/**
 * Free all the memory allocated for @array's internals (cache and
 * items).
 */
void free_array(struct array *array)
{
	int i;

	if (array == NULL || array->_cache == NULL)
		return;

	for (i = 0; i < array->length; i++)
		free_item(array, i);

	free(array->_cache);
	array->_cache = NULL;
	array->length = 0;
}
