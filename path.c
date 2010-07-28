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

#define FINAL_NORMAL    1
#define FINAL_FORCE_DIR 2

/**
 * Copy in @component the first path component pointed to by @cursor,
 * this later is updated to point to the next component for a further
 * call. Also, this function set @is_final to:
 *
 *     - FINAL_FORCE_DIR if it the last component of the path but we
 *       really expect a directory.
 *
 *     - FINAL_NORMAL if it the last component of the path.
 *
 *     - 0 otherwise.
 *
 * This function returns -errno if an error
 * occured, otherwise it returns 0.
 */
static inline int next_component(char component[NAME_MAX], const char **cursor, int *is_final)
{
	const char *start;
	ptrdiff_t length;
	int want_dir;

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

	/* Chec if a [link to a] directory is expected. */
	want_dir = (**cursor == '/');

	/* Skip trailing path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	if (**cursor == '\0')
		*is_final = (want_dir
			     ? FINAL_FORCE_DIR
			     : FINAL_NORMAL);

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
static int canonicalize(pid_t pid,
			const char *fake_path,
			int deref_final,
			char result[PATH_MAX],
			unsigned int nb_readlink,
			int check_isolation)
{
	const char *cursor;
	char tmp[PATH_MAX];
	int is_final;
	int status;

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
		struct stat statl;

		status = next_component(component, &cursor, &is_final);
		if (status < 0)
			return status;

		if (strcmp(component, ".") == 0)
			continue;

		if (strcmp(component, "..") == 0) {
			/* Check we are not popping beyond the new root. */
			if (check_isolation != 0
			    && strspn(result, "/") == strlen(result))
				return -EPERM;

			pop_component(result);
			continue;
		}

		/* Very special case: substitute "/proc/self" with "/proc/$pid".
		   The following check covers only 99.999% of the cases. */
		if (   strcmp(component, "self") == 0
		    && strcmp(result, "/proc")   == 0
		    && (!is_final || deref_final)) {
			status = sprintf(component, "%d", pid);
			if (status < 0)
				return -EPERM;
			if (status >= sizeof(component))
				return -EPERM;
		}

		status = join_paths(3, real_entry, root, result, component);
		if (status < 0)
			return status;

		status = lstat(real_entry, &statl);

		/* Nothing special to do if it's not a link or if we
		   explicitly ask to not dereference 'fake_path', as
		   required by syscalls like lstat(2). Obviously, this
		   later condition does not apply to intermediate path
		   components. Errors are explicitly ignored since
		   they should be handled by the caller. */
		if (status < 0
		    || !S_ISLNK(statl.st_mode)
		    || (is_final == FINAL_NORMAL && !deref_final)) {
			strcpy(tmp, result);
			status = join_paths(2, result, tmp, component);
			if (status < 0)
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

		/* Remove the leading "root" part if needed, it's
		 * usefull for "/proc/self/cwd/" for instance. */
		detranslate_path(real_entry, tmp, 0);

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link so it is relative to 'result'. */
		status = canonicalize(pid, real_entry, 1, result, ++nb_readlink, check_isolation);
		if (status != 0)
			return status;
	}

	/* Ensure we are accessing a directory. */
	if (is_final == FINAL_FORCE_DIR) {
		strcpy(tmp, result);
		status = join_paths(2, result, tmp, ".");
		if (status < 0)
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
	char cwd_link[32]; /* 32 > sizeof("/proc//cwd") + sizeof(#ULONG_MAX) */
	char tmp[PATH_MAX];
	int status;

	assert(initialized != 0);

	/* Check whether it is an sbolute path or not. */
	if (fake_path[0] != '/') {

		/* Format the path to the "virtual" link to child's cwd. */
		status = sprintf(cwd_link, "/proc/%d/cwd", pid);
		if (status < 0)
			return -EPERM;
		if (status >= sizeof(cwd_link))
			return -EPERM;

		/* Read the value of this "virtual" link. */
		status = readlink(cwd_link, tmp, PATH_MAX);
		if (status < 0)
			return -EPERM;
		if (status >= PATH_MAX)
			return -ENAMETOOLONG;
		tmp[status] = '\0';

		/* Ensure the current working directory is within the
		 * new root once the child process did a chdir(2). */
		if (strncmp(tmp, root, root_length) != 0) {
			fprintf(stderr, "proot: the child %d is out of my control (1)\n", pid);
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
	status = canonicalize(pid, fake_path, deref_final, result, 0, 0);
	if (status < 0)
		return status;

	/* Finally append the new root and the result of the
	 * canonicalization. */
	strcpy(tmp, result);
	status = join_paths(2, result, root, tmp);
	if (status < 0)
		return status;

	/* Add a small sanity check. It doesn't prove PRoot is secure! */
	if (deref_final != 0 && realpath(result, tmp) != NULL) {
		if (strncmp(tmp, root, root_length) != 0) {
			fprintf(stderr, "proot: the child %d is out of my control (2)\n", pid);
			return -EPERM;
		}
	}

	return 0;
}

/**
 * Remove the leading "root" part of a previously translated @path and
 * copies the result in @result. It returns 0 if the leading part was
 * not the "root" (@path is copied as-is into @result), otherwise it
 * returns the size in bytes of @result, including the end-of-string
 * terminator. On error it returns -errno.
 */
int detranslate_path(char result[PATH_MAX], const char path[PATH_MAX], int sanity_check)
{
	int new_length;

	/* Ensure the path is within the new root. */
	if (strncmp(path, root, root_length) != 0) {
		if (sanity_check != 0)
			return -EPERM;
		else {
			/* Copy the path as-is otherwise. */
			if (strlen(path) >= PATH_MAX)
				return -ENAMETOOLONG;
			strncpy(result, path, PATH_MAX);
			return 0;
		}
	}

	/* Remove the leading part, that is, the "root". */
	new_length = strlen(path) - root_length;
	if (new_length != 0) {
		memmove(result, path + root_length, new_length);
		result[new_length] = '\0';
	}
	else {
		/* Special case when path == root. */
		new_length = 1;
		strcpy(result, "/");
	}

	return new_length + 1;
}


/**
 * Check if the interpretation of @path relatively to the directory
 * referred to by the file descriptor @dirfd of the child process @pid
 * lands within the new "root". See the documentation of
 * translate_path() about the meaning of @deref_final. This function
 * returns -errno if an error occured, otherwise it returns 0.
 *
 * This function can't be called when AT_FD(@dirfd, @path) is false.
 */
int check_path_at(pid_t pid, int dirfd, char path[PATH_MAX], int deref_final)
{
	char old_base[PATH_MAX];
	char new_base[PATH_MAX];
	char fd_link[64]; /* 64 > sizeof("/proc//fd/") + 2 * sizeof(#ULONG_MAX) */
	int status;

	assert(AT_FD(dirfd, path));

	/* Format the path to the "virtual" link. */
	status = sprintf(fd_link, "/proc/%d/fd/%d", pid, dirfd);
	if (status < 0)
		return -EPERM;
	if (status >= sizeof(fd_link))
		return -EPERM;

	/* Read the value of this "virtual" link. */
	status = readlink(fd_link, old_base, PATH_MAX);
	if (status < 0)
		return -EPERM;
	if (status >= PATH_MAX)
		return -ENAMETOOLONG;
	old_base[status] = '\0';

	/* Ensure it points to a path (not a socket or somethink like that). */
	if (old_base[0] != '/')
		return -ENOTDIR;

	/* Remove the leading "root" part of the base (required!). */
	status = detranslate_path(new_base, old_base, 1);
	if (status < 0)
		return status;

	/* Check it this path lands outside of the new root. */
	status = canonicalize(pid, path, deref_final, new_base, 0, 1);
	if (status < 0)
		return status;

	return 0;
}
