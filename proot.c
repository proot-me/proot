/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (c) 2010 STMicroelectronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 */

#include <unistd.h>   /* readlink(2), lstat(2), readlink(2), getwd(3), */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strcmp(3), */
#include <limits.h>   /* PATH_MAX, */
#include <sys/stat.h> /* lstat(2), S_ISLNK(), */
#include <stdlib.h>   /* free(3), */
#include <errno.h>    /* errno(3), */
#include <stdarg.h>   /* va_*(3), */

#define MAX_READLINK 1024 /* TODO: extract from GlibC sources. */

/**
 * This function returns a buffer allocated with malloc(3) that
 * contains the first path component pointed to by @cursor, this later
 * updated to point to the next component for a further call. Also,
 * this function updates the write-only parameter @is_final to
 * indicate if it is the last component of the path.
 */
static char *next_component(const char **cursor, int *is_final)
{
	const char *component_start = NULL;
	const char *component_end = NULL;

	/* Sanity checks. */
	if (cursor == NULL || is_final == NULL) {
		errno = EINVAL;
		return NULL;
	}

	/* Skip leading path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	/* This very special case could happen only
	 * if fake_path is equivalent to '/'. */
	if (**cursor == '\0') {
		*is_final = 1;
		/* Errors and free(3) are handled by the caller. */
		return strdup(".");
	}

	/* Find the next component. */
	component_start = *cursor;
	while (**cursor != '\0' && **cursor != '/')
		(*cursor)++;
	component_end = *cursor;

	/* Skip trailing path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	*is_final = (**cursor == '\0');

	/* Errors and free(3) are handled by the caller. */
	return strndup(component_start, component_end - component_start);
}

/**
 * This function removes the last component of @path. Technically it
 * adds an end-of-string ('\0') right before the last component, thus
 * there is no memory management at all.
 */
static void pop_component(char *path)
{
	int offset = 0;

	/* Sanity checks. */
	if (path == NULL)
		return;

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
 * This functions concatenates a variadic number of paths, it adds a
 * path separator in between when needed.
 */
static char *join_paths(int number_paths, ...)
{
	char *result = NULL;
	va_list paths;
	int i = 0;

	/* Parse the list of variadic arugments. */
	va_start(paths, number_paths);
	for (i = 0; i < number_paths; i++) {
		const char *path   = NULL;
		int need_separator = 0;
		int length = 0;

		path = va_arg(paths, const char *);
		if (path == NULL)
			continue;

		/* This is the first iteration, so let's allocate some
		 * memory to copy the entry as is. */
		if (result == NULL) {
			result = strdup(path);
			continue;
		}

		/* Check if a path separator is needed. */
		length = strlen(result);
		if (result[length] != '/' && path[0] != '/') {
			need_separator = 1;
			length++;
		}
		length += strlen(path);

		result = realloc(result, length + 1);
		if (result == NULL) {
			/* Errors and free(3) are handled by the caller. */
			return NULL;
		}

		if (need_separator)
			strcat(result, "/");
		strcat(result, path);
	}
	va_end(paths);

	return result;
}

/**
 * This function returns a buffer allocated with malloc(3) that
 * contains the canonicalization (see `man 3 realpath`) of @fake_path
 * regarding to @new_root. The path to canonicalize could be either
 * absolute or relative to @relative_to. When the last component of
 * @fake_path is a link, it is dereferenced only if @deref_final is
 * true -- it is usefull for syscalls like lstat(2).
 *
 * The special value NULL is returned and errno is set either if a
 * broken link is encountered or if a component of a pathname exceeded
 * NAME_MAX characters or if the number of encountered symbolic links
 * @nb_readlink -- should be 0 at the initial call -- is greater than
 * MAX_READLINK or if there is not enough memory to do the job.
 */
char *canonicalize(const char *new_root,
		   const char *fake_path,
		   int deref_final,
		   const char *relative_to,
		   int nb_readlink)
{
	char *result;
	const char *cursor = NULL;
	int is_final = 0;

#ifdef DEBUG
#include <stdio.h>
	printf("%s %s %d %s %d\n", new_root, fake_path, deref_final, relative_to, nb_readlink);
#endif

	/* Avoid infinite loop on circular links. */
	if (nb_readlink > MAX_READLINK) {
		errno = ELOOP;
		return NULL;
	}

	/* Sanity checks. */
	if (fake_path == NULL || new_root == NULL || new_root[0] != '/') {
		errno = EINVAL;
		return NULL;
	}

	/* Absolutize relative 'fake_path'. */
	if (fake_path[0] != '/') {
		if (relative_to == NULL || relative_to[0] != '/') {
			errno = EINVAL;
			return NULL;
		}
		result = strdup(relative_to);
	}
	else
		result = strdup("");

	if (result == NULL) {
		/* errno is already set. */
		return NULL;
	}

	/* Canonicalize recursely 'fake_path' into 'result'. */
	cursor = fake_path;
	while (!is_final) {
		char *current = NULL;
		char *real_entry = NULL;
		char *old_result = NULL;
		char referee[PATH_MAX];
		struct stat statl;
		int status = 0;

		current = next_component(&cursor, &is_final);
		if (current == NULL) {
			/* errno is already set. */
			free(result);
			result = NULL;
			goto skip;
		}

		if (strcmp(current, ".") == 0)
			goto skip;

		if (strcmp(current, "..") == 0) {
			pop_component(result);
			goto skip;
		}

		real_entry = join_paths(3, new_root, result, current);
		if (real_entry == NULL) {
			/* errno is already set. */
			free(result);
			result = NULL;
			goto skip;
		}
		status = lstat(real_entry, &statl);

		/* Nothing special to do if it's not a link or if we
		   explicitly ask to not dereference 'fake_path', as
		   required by syscalls like lstat(2). Obviously, this
		   later condition does not apply to intermediate path
		   components. Errors are explicitly ignored since
		   they should be handled by the caller. */
		if (status < 0
		    || !S_ISLNK(statl.st_mode)
		    || (is_final && !deref_final)) {
			old_result = result;
			result = join_paths(2, old_result, current);
			free(old_result);
			goto skip;
		}

		/* It's a link, so we have to dereference *and*
		   canonicalize to ensure we are not going outside the
		   new root. */
		status = readlink(real_entry, referee, sizeof(referee));
		if (status == -1 || status == sizeof(referee)) {
			/* errno is already set except when the
			   content is truncated to sizeof(referee). */
			free(result);
			result = NULL;
			goto skip;
		}
		referee[sizeof(referee) - 1] = '\0';

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link then it is relative to 'result'. */
		old_result = result;
		result = canonicalize(new_root, referee, 1, old_result, ++nb_readlink);
		free(old_result);

	skip:
		free(current);

		if (real_entry != NULL)
			free(real_entry);

		/* An exception occured due to either a broken link or
		 * a path name too long. */
		if (result == NULL)
			break;
	}

	return result;
}

/**
 * This function returns a buffer allocated with malloc(3) that
 * contains basically "join(@new_root, canonicalize(@fake_path))".
 * See the documentation of canonicalize() for the meaning of
 * @deref_final.
 */
char *proot(const char *new_root, const char *fake_path, int deref_final)
{
	char *result1  = NULL;
	char *result2  = NULL;
	char *fake_cwd = NULL;
	char real_root[PATH_MAX];
	char cwd[PATH_MAX];
	int length = 0;

	if (realpath(new_root, real_root) == NULL)
		return NULL;

	if (fake_path[0] == '/') {
		fake_cwd = NULL;
	}
	else {
		if (getcwd(cwd, PATH_MAX) == NULL)
			return NULL;

		/* Ensure the current working directory is within the new root. */
		length = strlen(real_root);
		if (strncmp(cwd, real_root, length) != 0) {
			errno = EPERM;
			return NULL;
		}

		fake_cwd = cwd + length;

		/* Special case when cwd == real_root. */
		if (fake_cwd[0] == '\0')
			fake_cwd = "/";
	}

	result1 = canonicalize(real_root, fake_path, deref_final, fake_cwd, 0);
	if (result1 == NULL)
		return NULL;

	result2 = join_paths(2, real_root, result1);
	free(result1);

	return result2;
}
