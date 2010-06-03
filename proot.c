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
	assert(cursor   != NULL);
	assert(is_final != NULL);

	/* Skip leading path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	/* This very special case could happen only
	 * if fake_path is equivalent to '/'. */
	if (**cursor == '\0') {
		*is_final = 1;
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

	/* Errors and freeing are handled by the caller. */
	return strndup(component_start, component_end - component_start);
}

static void pop_component(char *path) { }
static void push_component(char *path, char *component) { }
static char *join_components(const char *a, ...) { return NULL; }

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

	/* Sanity checks. */
	assert(fake_path != NULL);
	assert(new_root  != NULL && new_root[0] == '/');

	/* Absolutize relative 'fake_path'. */
	if (fake_path[0] != '/') {
		assert(relative_to != NULL);
		assert(relative_to[0] == '/');

		result = strdup(relative_to);
	}
	else
		result = strdup("");

	if (result == NULL)
		return NULL;

	/* Canonicalize recursely 'fake_path' into 'result'. */
	cursor = fake_path;
	while (!is_final) {
		char *current = NULL;
		char *real_entry = NULL;
		char referee[PATH_MAX];
		struct stat statl;
		int status = 0;

		current = next_component(&cursor, &is_final);
		if (current == NULL) {
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

		real_entry = join_components(new_root, result, current);
		if (real_entry == NULL) {
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
			push_component(result, current);
			goto skip;
		}

		/* It's a link, so we have to dereference *and*
		   canonicalize to ensure we are not going outside the
		   new root. */
		status = readlink(real_entry, referee, sizeof(referee));
		if (status == -1
		    || status == sizeof(referee)
		    || nb_readlink > MAX_READLINK) {
			errno = ELOOP;
			free(result);
			result = NULL;
			goto skip;
		}
		referee[sizeof(referee) - 1] = '\0';

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link then it is relative to 'result'. */
		result = canonicalize(new_root, referee, 0, result, nb_readlink++);

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
	char *result1 = NULL;
	char *result2 = NULL;
	char cwd[PATH_MAX];

	if (getcwd(cwd, PATH_MAX) == NULL)
		return NULL;

	result1 = canonicalize(new_root, fake_path, deref_final, cwd, 0);
	if (result1 == NULL)
		return NULL;

	result2 = join_components(new_root, result1);
	free(result1);

	return result2;
}
