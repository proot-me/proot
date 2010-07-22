/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010 STMicroelectronics
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
 * Inspired by: realpath() from the GNU C Library.
 */

#include <unistd.h>   /* readlink(2), lstat(2), readlink(2) */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strcmp(3), strcpy(3), strncpy(3), memmove(3), */
#include <limits.h>   /* PATH_MAX, */
#include <sys/stat.h> /* lstat(2), S_ISLNK(), */
#include <stdlib.h>   /* realpath(3), exit(3), EXIT_*, */
#include <errno.h>    /* errno(3), */
#include <stdarg.h>   /* va_*(3), */
#include <sys/param.h> /* MAXSYMLINKS, */
#include <stddef.h>   /* ptrdiff_t, */
#include <stdio.h>    /* perror(3), */
#include <sys/types.h> /* pid_t, */

#include "path.h"
#include "child.h" /* get_child_cwd(), */

static int  initialized = 0;
static char root[PATH_MAX];
static size_t root_length;

/* Tnitialize internal data of the path translator. */
void init_path_translator(const char *new_root)
{
	if (realpath(new_root, root) == NULL) {
		perror("proot -- realpath()");
		exit(EXIT_FAILURE);
	}

	root_length = strlen(root);
	initialized = 1;
}

/**
 * Copy in @component the first path component pointed to by @cursor,
 * this later is updated to point to the next component for a further
 * call. Also, this function set @is_final to true if it is the last
 * component of the path. This function returns -errno if an error
 * occured, otherwise it returns 0.
 */
static inline int next_component(char component[NAME_MAX], const char **cursor, int *is_final)
{
	const char *start;
	ptrdiff_t length;

	/* Sanity checks. */
	assert(component != NULL);
	assert(is_final  != NULL);
	assert(cursor    != NULL);

	/* Skip leading path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	/* Find the next component. */
	start = *cursor;
	while (**cursor != '\0' && **cursor != '/')
		(*cursor)++;
	length = *cursor - start;

	if (length >= NAME_MAX)
		return -ENAMETOOLONG;

	/* Extract the component. */
	strncpy(component, start, length);
	component[length] = '\0';

	/* Skip trailing path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	*is_final = (**cursor == '\0');
	return 0;
}

/**
 * Put an end-of-string ('\0') right before the last component of @path.
 */
static inline void pop_component(char *path)
{
	int offset;

	/* Sanity checks. */
	assert(path != NULL);

	offset = strlen(path) - 1;

	/* Skip trailing path separators. */
	while (offset > 0 && path[offset] == '/')
		offset--;

	/* Search for the previous path separator. */
	while (offset > 0 && path[offset] != '/')
		offset--;

	/* Cut the end of the string before the last component. */
	path[offset] = '\0';
}

/**
 * Copy in @result the concatenation of several paths (@number_paths)
 * and adds a path separator ('/') in between when needed. This
 * function returns -errno if an error occured, otherwise it returns 0.
 */
static inline int join_paths(int number_paths, char result[PATH_MAX], ...)
{
	va_list paths;
	size_t length;
	int i = 0;

	result[0] = '\0';

	/* Parse the list of variadic arguments. */
	va_start(paths, result);
	length = 0;
	for (i = 0; i < number_paths; i++) {
		const char *path;
		int need_separator;
		size_t old_length;

		path = va_arg(paths, const char *);
		if (path == NULL)
			continue;

		/* Check if a path separator is needed. */
		if (length > 0 && result[length - 1] != '/' && path[0] != '/')
			need_separator = 1;
		else
			need_separator = 0;

		old_length = length;
		length += strlen(path) + need_separator;
		if (length >= PATH_MAX) {
			va_end(paths);
			return -ENAMETOOLONG;
		}

		if (need_separator != 0) {
			strcat(result + old_length, "/");
			old_length++;
		}
		strcat(result + old_length, path);
	}
	va_end(paths);

	return 0;
}

/**
 * Copy in @result the anonicalization (see `man 3 realpath`) of
 * @fake_path regarding to @root. The path to canonicalize could be
 * either absolute or relative to @result. When the last component of
 * @fake_path is a link, it is dereferenced only if @deref_final is
 * true -- it is usefull for syscalls like lstat(2). The parameter
 * @nb_readlink should be set to 0 unless you know what you are
 * doing. This function returns -errno if an error occured, otherwise
 * it returns 0.
 */
static int canonicalize(const char *fake_path,
			int deref_final,
			char result[PATH_MAX],
			unsigned int nb_readlink)
{
	const char *cursor;
	int is_final;

	/* Avoid infinite loop on circular links. */
	if (nb_readlink > MAXSYMLINKS)
		return -ELOOP;

	/* Sanity checks. */
	assert(fake_path != NULL);
	assert(result != NULL);

	if (fake_path[0] != '/') {
		/* Ensure 'result' contains an absolute base of the relative fake path. */
		if (result[0] != '/')
			return -EINVAL;
	}
	else
		strcpy(result, "/");

	/* Canonicalize recursely 'fake_path' into 'result'. */
	cursor = fake_path;
	is_final = 0;
	while (!is_final) {
		char component[NAME_MAX];
		char real_entry[PATH_MAX];
		char tmp[PATH_MAX];
		struct stat statl;
		int status;

		status = next_component(component, &cursor, &is_final);
		if (status != 0)
			return status;

		if (strcmp(component, ".") == 0)
			continue;

		if (strcmp(component, "..") == 0) {
			pop_component(result);
			continue;
		}

		status = join_paths(3, real_entry, root, result, component);
		if (status != 0)
			return status;

		status = lstat(real_entry, &statl);

		/* Nothing special to do if it's not a link or if we
		   explicitly ask to not dereference 'fake_path', as
		   required by syscalls like lstat(2). Obviously, this
		   later condition does not apply to intermediate path
		   components. Errors are explicitly ignored since
		   they should be handled by the caller. */
		if (status < 0 || !S_ISLNK(statl.st_mode) || (is_final && !deref_final)) {
			strcpy(tmp, result);
			status = join_paths(2, result, tmp, component);
			if (status != 0)
				return status;
			continue;
		}

		/* It's a link, so we have to dereference *and*
		   canonicalize to ensure we are not going outside the
		   new root. */
		status = readlink(real_entry, tmp, sizeof(tmp));
		if (status < 0)
			return status;
		else if (status == sizeof(tmp))
			return -ENAMETOOLONG;
		else
			tmp[status] = '\0';

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link so it is relative to 'result'. */
		status = canonicalize(tmp, 1, result, ++nb_readlink);
		if (status != 0)
			return status;
	}

	return 0;
}

/**
 * Copy in @result the equivalent of @root/canonicalize(@fake_path)).
 * If @fake_path is not absolute then it is relative to the current
 * working directory. See the documentation of canonicalize() for the
 * meaning of @deref_final.
 */
int translate_path(pid_t pid, char result[PATH_MAX], const char *fake_path, int deref_final)
{
	char tmp[PATH_MAX];
	int status;

	assert(initialized != 0);

	/* Check whether it is an sbolute path or not. */
	if (fake_path[0] != '/') {
		status = get_child_cwd(pid, tmp);
		if (status < 0)
			return status;

		/* Ensure the current working directory is within the
		 * new root once the child process did a chdir(2). */
		if (strncmp(tmp, root, root_length) != 0) {
			fprintf(stderr, "proot: the child %d is out of my control\n", pid);
			return -EPERM;
		}

		strcpy(result, tmp + root_length);

		/* Special case when child's cwd == root. */
		if (result[0] == '\0')
			strcpy(result, "/");
	}
	else
		strcpy(result, "/");

	/* Canonicalize regarding the new root. */
	status = canonicalize(fake_path, deref_final, result, 0);
	if (status != 0)
		return status;

	/* Finally append the new root and the result of the
	 * canonicalization. */
	strcpy(tmp, result);
	status = join_paths(2, result, root, tmp);
	if (status != 0)
		return status;

	return 0;
}

/**
 * Removes the leading "root" part of a previously translated @path
 * and copies the result in @result. It returns the number in bytes of
 * the string, including the end-of-string terminator.
 */
int detranslate_path(char result[PATH_MAX], const char *path)
{
	size_t length = 0;
	size_t new_length = 0;

	/* Ensure the path is within the new root. */
	if (strncmp(path, root, root_length) != 0)
		return -EPERM;

	strcpy(result, path);
	length = strlen(result);

	/* Ensure it fits into the result buffer. */
	new_length = length - root_length;
	if (new_length >= PATH_MAX)
		return -ENAMETOOLONG;

	/* Remove the leading part, that is, the "root". */
	if (new_length != 0) {
		memmove(result, result + root_length, strlen(result));
		result[new_length] = '\0';
	}
	else {
		/* Special case when path == root. */
		new_length = 1;
		strcpy(result, "/");
	}

	return new_length + 1;
}
