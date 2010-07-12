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

#include <unistd.h>   /* readlink(2), lstat(2), readlink(2), getwd(3), */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strcmp(3), strcpy(3), strncpy(3), */
#include <limits.h>   /* PATH_MAX, */
#include <sys/stat.h> /* lstat(2), S_ISLNK(), */
#include <stdlib.h>   /* realpath(3), */
#include <errno.h>    /* errno(3), */
#include <stdarg.h>   /* va_*(3), */
#include <sys/param.h> /* MAXSYMLINKS, */
#include <stddef.h>   /* ptrdiff_t, */

/**
 * This function copies in @component the first path component pointed
 * to by @cursor, this later is updated to point to the next component
 * for a further call. Also, this function set @is_final to true if it
 * is the last component of the path. This function returns -1 on
 * error and errno is set, otherwise it returns 0.
 */
static inline int next_component(char component[NAME_MAX], const char **cursor, int *is_final)
{
	const char *start = NULL;
	ptrdiff_t length  = 0;

	/* Sanity checks. */
	assert(component != NULL && cursor != NULL && is_final != NULL);

	/* Skip leading path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	/* Find the next component. */
	start = *cursor;
	while (**cursor != '\0' && **cursor != '/')
		(*cursor)++;
	length = *cursor - start;

	if (length >= NAME_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

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
 * This function puts end-of-string ('\0') right before the last
 * component of @path.
 */
static inline void pop_component(char *path)
{
	int offset = 0;

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
 * This functions copies in @result the concatenation of several paths
 * (@number_paths) and adds a path separator ('/') in between when
 * needed. This function returns -1 on error and errno is set,
 * otherwise it returns 0.
 */
static inline int join_paths(int number_paths, char result[PATH_MAX], ...)
{
	va_list paths;
	size_t length = 0;
	int i = 0;

	result[0] = '\0';

	/* Parse the list of variadic arguments. */
	va_start(paths, result);
	for (i = 0; i < number_paths; i++) {
		const char *path   = NULL;
		int need_separator = 0;
		size_t old_length  = 0;

		path = va_arg(paths, const char *);
		if (path == NULL)
			continue;

		/* Check if a path separator is needed. */
		if (length > 0 && result[length - 1] != '/' && path[0] != '/')
			need_separator = 1;

		old_length = length;
		length += strlen(path) + need_separator;
		if (length >= PATH_MAX) {
			errno = ENAMETOOLONG;
			va_end(paths);
			return -1;
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
 * This function copies in @result the anonicalization (see `man 3
 * realpath`) of @fake_path regarding to @new_root. The path to
 * canonicalize could be either absolute or relative to @result. When
 * the last component of @fake_path is a link, it is dereferenced only
 * if @deref_final is true -- it is usefull for syscalls like
 * lstat(2). The parameter @nb_readlink should be set to 0 unless you
 * know what you are doing. This function returns -1 on error and
 * errno is set, otherwise it returns 0.
 */
static int canonicalize(const char *new_root,
			const char *fake_path,
			int deref_final,
			char result[PATH_MAX],
			unsigned int nb_readlink)
{
	const char *cursor = NULL;
	int is_final = 0;

	/* Avoid infinite loop on circular links. */
	if (nb_readlink > MAXSYMLINKS) {
		errno = ELOOP;
		return -1;
	}

	/* Sanity checks. */
	if (  fake_path == NULL || result == NULL
	    || new_root == NULL || new_root[0] != '/') {
		errno = EINVAL;
		return -1;
	}

	if (fake_path[0] != '/') {
		/* Ensure 'result' contains an absolute base of the relative fake path. */
		if (result[0] != '/') {
			errno = EINVAL;
			return -1;
		}
	}
	else
		strcpy(result, "/");

	/* Canonicalize recursely 'fake_path' into 'result'. */
	cursor = fake_path;
	while (!is_final) {
		char component[NAME_MAX];
		char real_entry[PATH_MAX];
		char tmp[PATH_MAX];
		struct stat statl;
		int status = 0;

#ifdef DEBUG
#include <stdio.h>
	printf("%s %s %d %s %d\n", new_root, cursor, deref_final, result, nb_readlink);
#endif
		status = next_component(component, &cursor, &is_final);
		if (status != 0)
			return -1; /* errno is already set. */

		if (strcmp(component, ".") == 0)
			continue;

		if (strcmp(component, "..") == 0) {
			pop_component(result);
			continue;
		}

		status = join_paths(3, real_entry, new_root, result, component);
		if (status != 0)
			return -1; /* errno is already set. */

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
				return -1; /* errno is already set. */
			continue;
		}

		/* It's a link, so we have to dereference *and*
		   canonicalize to ensure we are not going outside the
		   new root. */
		status = readlink(real_entry, tmp, sizeof(tmp));
		switch (status) {
		case -1:
			return -1; /* errno is already set. */

		case sizeof(tmp):
			/* The content is truncated. */
			errno = ENAMETOOLONG;
			return -1;

		default:
			/* Terminate correctly the string. */
			tmp[status] = '\0';
		}

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link so it is relative to 'result'. */
		status = canonicalize(new_root, tmp, 1, result, ++nb_readlink);
		if (status != 0)
			return -1; /* errno is set. */
	}

	return 0;
}

/**
 * This function copies in @result the equivalent of "@new_root /
 * canonicalize(@fake_path))". If @fake_path is not absolute then it
 * is relative to the current working directory. See the documentation
 * of canonicalize() for the meaning of @deref_final.
 */
int proot(char result[PATH_MAX], const char *new_root, const char *fake_path, int deref_final)
{
	char real_root[PATH_MAX];
	char tmp[PATH_MAX];
	int length = 0;
	int status = 0;

	if (realpath(new_root, real_root) == NULL)
		return -1; /* errno is already set. */

	if (fake_path[0] != '/') {
		if (getcwd(tmp, PATH_MAX) == NULL)
			return -1; /* errno is already set. */

		/* Ensure the current working directory is within the new root. */
		length = strlen(real_root);
		if (strncmp(tmp, real_root, length) != 0) {
			errno = EPERM;
			return -1;
		}

		strcpy(result, tmp + length);

		/* Special case when cwd == real_root. */
		if (result[0] == '\0')
			strcpy(result, "/");
	}
	else
		strcpy(result, "/");

	status = canonicalize(real_root, fake_path, deref_final, result, 0);
	if (status != 0)
		return -1; /* errno is already set. */

	strcpy(tmp, result);
	status = join_paths(2, result, real_root, tmp);
	if (status != 0)
		return -1; /* errno is already set. */

	return 0;
}
